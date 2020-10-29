#include <ripple/app/tx/applyHook.h>
#include <ripple/basics/Log.h>
#include <ripple/basics/Slice.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/ledger/OpenLedger.h>
#include <ripple/app/misc/TxQ.h>
#include <ripple/app/misc/NetworkOPs.h>

using namespace ripple;

bool hook::canHook(ripple::TxType txType, uint64_t hookOn) {
    // invert ttHOOK_SET bit
    hookOn ^= (1ULL << ttHOOK_SET);
    // invert entire field
    hookOn ^= 0xFFFFFFFFFFFFFFFFULL;
    return (hookOn >> txType) & 1;
}



TER
hook::setHookState(
    HookContext& hookCtx,
    Keylet const& hookStateKeylet,
    Slice& data
){

    auto& view = hookCtx.applyCtx.view();
    auto j = hookCtx.applyCtx.app.journal("View");
    auto const sle = view.peek(hookCtx.accountKeylet);
    if (!sle)
        return tefINTERNAL;

    auto const hook = view.peek(hookCtx.hookKeylet);
    if (!hook) { // [RH] should this be more than trace??
        JLOG(j.trace()) << "Attempted to set a hook state for a hook that doesnt exist " << toBase58(hookCtx.account);
        return tefINTERNAL;
    }

    uint32_t hookDataMax = hook->getFieldU32(sfHookDataMaxSize);

    // if the blob is too large don't set it
    if (data.size() > hookDataMax) {
       return temHOOK_DATA_TOO_LARGE;
    }

    uint32_t stateCount = hook->getFieldU32(sfHookStateCount);
    uint32_t oldStateReserve = COMPUTE_HOOK_DATA_OWNER_COUNT(stateCount);

    auto const oldHookState = view.peek(hookStateKeylet);

    // if the blob is nil then delete the entry if it exists
    if (data.size() == 0) {

        if (!view.peek(hookStateKeylet))
            return tesSUCCESS; // a request to remove a non-existent entry is defined as success

        auto const hint = (*oldHookState)[sfOwnerNode];

        // Remove the node from the account directory.
        if (!view.dirRemove(hookCtx.ownerDirKeylet, hint, hookStateKeylet.key, false))
        {
            return tefBAD_LEDGER;
        }

        // remove the actual hook state obj
        view.erase(oldHookState);

        // adjust state object count
        if (stateCount > 0)
            --stateCount; // guard this because in the "impossible" event it is already 0 we'll wrap back to int_max

        // if removing this state entry would destroy the allotment then reduce the owner count
        if (COMPUTE_HOOK_DATA_OWNER_COUNT(stateCount) < oldStateReserve)
            adjustOwnerCount(view, sle, -1, j);

        hook->setFieldU32(sfHookStateCount, COMPUTE_HOOK_DATA_OWNER_COUNT(stateCount));

        return tesSUCCESS;
    }


    std::uint32_t ownerCount{(*sle)[sfOwnerCount]};

    if (oldHookState) {
        view.erase(oldHookState);
    } else {

        ++stateCount;

        if (COMPUTE_HOOK_DATA_OWNER_COUNT(stateCount) > oldStateReserve) {
            // the hook used its allocated allotment of state entries for its previous ownercount
            // increment ownercount and give it another allotment

            ++ownerCount;
            XRPAmount const newReserve{
                view.fees().accountReserve(ownerCount)};

            if (STAmount((*sle)[sfBalance]).xrp() < newReserve)
                return tecINSUFFICIENT_RESERVE;


            adjustOwnerCount(view, sle, 1, j);

        }

        // update state count
        hook->setFieldU32(sfHookStateCount, stateCount);

    }

    // add new data to ledger
    auto newHookState = std::make_shared<SLE>(hookStateKeylet);
    view.insert(newHookState);
    newHookState->setFieldVL(sfHookData, data);

    if (!oldHookState) {
        // Add the hook to the account's directory if it wasn't there already
        auto const page = dirAdd(
            view,
            hookCtx.ownerDirKeylet,
            hookStateKeylet.key,
            false,
            describeOwnerDir(hookCtx.account),
            j);

        JLOG(j.trace()) << "Create/update hook state for account " << toBase58(hookCtx.account)
                     << ": " << (page ? "success" : "failure");

        if (!page)
            return tecDIR_FULL;

        newHookState->setFieldU64(sfOwnerNode, *page);

    }

    return tesSUCCESS;
}

void hook::printWasmerError()
{
  int error_len = wasmer_last_error_length();
  char *error_str = (char*)malloc(error_len);
  wasmer_last_error_message(error_str, error_len);
  printf("Error: `%s`\n", error_str);
    free(error_str);
}

TER hook::apply(Blob hook, ApplyContext& applyCtx, const AccountID& account, bool callback = false) {

    wasmer_instance_t *instance = NULL;

    if (wasmer_instantiate(&instance,
                hook.data(), hook.size(), imports, imports_count) != wasmer_result_t::WASMER_OK) {
        printf("hook malformed\n");
        printWasmerError();
        return temMALFORMED;
    }

    HookContext hookCtx {
        .applyCtx = applyCtx,
        .account = account,
        .accountKeylet = keylet::account(account),
        .ownerDirKeylet = keylet::ownerDir(account),
        .hookKeylet = keylet::hook(account),
        .changedState =
            std::make_shared<std::map<ripple::uint256 const, std::pair<bool, ripple::Blob>>>(),
        .exitType = hook_api::ExitType::ROLLBACK, // default is to rollback unless hook calls accept() or reject()
        .exitReason = std::string(""),
        .exitCode = -1
    };

    wasmer_instance_context_data_set ( instance, &hookCtx );
    printf("Set HookContext: %lx\n", (void*)&hookCtx);

    wasmer_value_t arguments[] = { { .tag = wasmer_value_tag::WASM_I64, .value = {.I64 = 0 } } };
    wasmer_value_t results[] = { { .tag = wasmer_value_tag::WASM_I64, .value = {.I64 = 0 } } };

    wasmer_instance_call(
        instance,
        (!callback ? "hook" : "cbak"),
        arguments,
        1,
        results,
        1
    );

    printf( "hook exit type was: %s\n",
            ( hookCtx.exitType == hook_api::ExitType::ROLLBACK ? "ROLLBACK" :
            ( hookCtx.exitType == hook_api::ExitType::ACCEPT ? "ACCEPT" : "REJECT" ) ) );

    printf( "hook exit reason was: `%s`\n", hookCtx.exitReason.c_str() );

    printf( "hook exit code was: %d\n", hookCtx.exitCode );

    printf( "hook ledger no: %d\n", hookCtx.applyCtx.view().info().seq);

    if (hookCtx.exitType != hook_api::ExitType::ROLLBACK) {
        printf("Committing changes made by hook\n");
        commitChangesToLedger(hookCtx);
    }


    // todo: [RH] memory leak here, destroy the imports, instance using a smart pointer
    wasmer_instance_destroy(instance);
    printf("running hook 3\n");

    if (hookCtx.exitType == hook_api::ExitType::ACCEPT) {
        return tesSUCCESS;
    } else {
        return tecHOOK_REJECTED;
    }
}


int64_t hook_api::trace_num (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t read_ptr, uint32_t read_len, int64_t number )
{

    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx on current stack

    printf("HOOKAPI_trace_num: ");
    if (read_len > 1024) read_len = 1024;
    for (int i = 0; i < read_len && i < memory_length; ++i)
    {
        printf("%c", memory[read_ptr + i]);
    }


    printf(": %lld\n", number);
    return read_len;

}

int64_t hook_api::trace (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t read_ptr, uint32_t read_len, uint32_t as_hex )
{

    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx on current stack

    printf("HOOKAPI_trace: ");
    if (read_len > 1024) read_len = 1024;
    for (int i = 0; i < read_len && i < memory_length; ++i)
    {
        if (as_hex)
            printf("%02X", memory[read_ptr + i]);
        else
            printf("%c", memory[read_ptr + i]);
    }
    printf("\n");
    return read_len;

}

int64_t hook_api::state_set (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t read_ptr, uint32_t read_len,
        uint32_t kread_ptr, uint32_t kread_len )
{

    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    if (kread_ptr + 32 > memory_length || read_ptr + hook::maxHookDataSize() > memory_length) {
        JLOG(j.trace())
            << "Hook tried to state_set using memory outside of the wasm instance limit";
        return OUT_OF_BOUNDS;
    }

    if (read_len == 0)
        return TOO_SMALL;

    auto const sle = view.peek(hookCtx.hookKeylet);
    if (!sle)
        return INTERNAL_ERROR;

    uint32_t maxSize = sle->getFieldU32(sfHookDataMaxSize);
    if (read_len > maxSize)
        return TOO_BIG;

    ripple::uint256 key = ripple::uint256::fromVoid(memory + kread_ptr);

    (*hookCtx.changedState)[key] =
        std::pair<bool, ripple::Blob> (true,
                {memory + read_ptr,  memory + read_ptr + read_len});

    return read_len;

}


void hook::commitChangesToLedger ( HookContext& hookCtx ) {

    // first write all changes to state

    for (const auto& cacheEntry : *(hookCtx.changedState)) {
        bool is_modified = cacheEntry.second.first;
        const auto& key = cacheEntry.first;
        const auto& blob = cacheEntry.second.second;
        if (is_modified) {
            // this entry isn't just cached, it was actually modified
            auto HSKeylet = keylet::hook_state(hookCtx.account, key);
            auto slice = Slice(blob.data(), blob.size());
            setHookState(hookCtx, HSKeylet, slice); // should not fail because checks were done before map insertion
        }
    }

    printf("emitted txn count: %d\n", hookCtx.emitted_txn.size());

    auto & netOps = hookCtx.applyCtx.app.getOPs();
    for (; hookCtx.emitted_txn.size() > 0; hookCtx.emitted_txn.pop())
    {
        auto& tpTrans = hookCtx.emitted_txn.front();
        std::cout << "submitting emitted tx: " << tpTrans << "\n";
        try
        {
            netOps.processTransaction(
                tpTrans, false, false, true, NetworkOPs::FailHard::yes);
        }
        catch (std::exception& e)
        {
            std::cout << "EMITTED TX FAILED TO PROCESS: " << e.what() << "\n";
        }
    }

}

int64_t hook_api::state (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t write_ptr, uint32_t write_len,
        uint32_t kread_ptr, uint32_t kread_len )
{
    return hook_api::state_foreign( wasm_ctx,
            write_ptr, write_len,
            kread_ptr, kread_len,
            0, 0);
}

/* this api actually serves both local and foreign state requests
 * feeding aread_ptr = 0 and aread_len = 0 will cause it to read local */
int64_t hook_api::state_foreign (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t write_ptr, uint32_t write_len,
        uint32_t kread_ptr, uint32_t kread_len,
        uint32_t aread_ptr, uint32_t aread_len )
{

    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    bool is_foreign = false;
    if (aread_ptr == 0 && aread_len == 0)
    {
        // valid arguments, local state
    } else if (aread_ptr > 0 && aread_len > 0)
    {
        // valid arguments, foreign state
        is_foreign = true;
    } else return INVALID_ARGUMENT;

    if (kread_ptr + kread_len > memory_length ||
        aread_ptr + aread_len > memory_length || // if ain/aread_len are 0 this will always be true
        write_ptr + write_len > memory_length)
    {
        JLOG(j.trace())
            << "Hook tried to state using memory outside of the wasm instance limit";
        return OUT_OF_BOUNDS;
    }

    if (kread_len > 32)
        return TOO_BIG;

    if (aread_len != 20)
        return INVALID_ACCOUNT;

    unsigned char key_padded[32];
    for (int i = 0; i < 32; ++i)
        key_padded[i] = ( i < kread_len ? *(memory + i + kread_ptr) : 0 );

    ripple::uint256 key = ripple::uint256::fromVoid(key_padded);


    // first check if the requested state was previously cached this session
    if (!is_foreign) // we only cache local
    {
        const auto& cacheEntry = hookCtx.changedState->find(key);
        if (cacheEntry != hookCtx.changedState->end())
        {
            if (cacheEntry->second.second.size() > write_len)
                return TOO_SMALL;

            WRITE_WASM_MEMORY_AND_RETURN(
                write_ptr, write_len,
                cacheEntry->second.second.data(), cacheEntry->second.second.size(),
                memory, memory_length);
        }
    }

    // cache miss look it up
    auto const sle = view.peek(hookCtx.hookKeylet);
    if (!sle)
        return INTERNAL_ERROR;

    auto hsSLE = view.peek(keylet::hook_state(
                (is_foreign ? AccountID::fromVoid(memory + aread_ptr) : hookCtx.account), key));
    if (!hsSLE)
        return DOESNT_EXIST;

    Blob b = hsSLE->getFieldVL(sfHookData);

    // it exists add it to cache and return it

    if (!is_foreign)
        hookCtx.changedState->emplace(key, std::pair<bool, ripple::Blob>(false, b));

    if (b.size() > write_len)
        return TOO_SMALL;

    WRITE_WASM_MEMORY_AND_RETURN(
        write_ptr, write_len,
        b.data(), b.size(),
        memory, memory_length);
}



int64_t hook_api::accept (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t read_ptr, uint32_t read_len,
        int32_t error_code )
{
    return hook_api::_exit(wasm_ctx, read_ptr, read_len, error_code, hook_api::ExitType::ACCEPT);
}

int64_t hook_api::reject (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t read_ptr, uint32_t read_len,
        int32_t error_code )
{
    return hook_api::_exit(wasm_ctx, read_ptr, read_len, error_code, hook_api::ExitType::REJECT);
}

int64_t hook_api::rollback (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t read_ptr, uint32_t read_len,
        int32_t error_code )
{
    return hook_api::_exit(wasm_ctx, read_ptr, read_len, error_code, hook_api::ExitType::ROLLBACK);
}

int64_t hook_api::_exit (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t read_ptr, uint32_t read_len,
        int32_t error_code, hook_api::ExitType exitType )
{

    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    if (read_ptr) {
        if (NOT_IN_BOUNDS(read_ptr, read_len, memory_length)) {
            JLOG(j.trace())
                << "Hook tried to accept/reject/rollback but specified memory outside of the wasm instance limit when specifying a reason string";
            return OUT_OF_BOUNDS;
        }

        hookCtx.exitReason = std::string ( (const char*)(memory + read_ptr), (size_t)read_len  );
    }

    hookCtx.exitType = exitType;
    hookCtx.exitCode = error_code;

    wasmer_raise_runtime_error(0, 0);

    // unreachable
    return 0;

}


int64_t hook_api::otxn_id (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t write_ptr, uint32_t write_len )
{

    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    auto const& txID = applyCtx.tx.getTransactionID();

    if (txID.size() > write_len)
        return TOO_SMALL;

    if (NOT_IN_BOUNDS(write_ptr, txID.size(), memory_length))
        return OUT_OF_BOUNDS;

    WRITE_WASM_MEMORY_AND_RETURN(
        write_ptr, txID.size(),
        txID.data(), txID.size(),
        memory, memory_length);
}

int64_t hook_api::otxn_type ( wasmer_instance_context_t * wasm_ctx )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    return applyCtx.tx.getTxnType();
}

int64_t hook_api::otxn_burden ( wasmer_instance_context_t * wasm_ctx )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    if (hookCtx.burden)
        return hookCtx.burden;

    auto const& tx = applyCtx.tx;
    if (!tx.isFieldPresent(sfEmitDetails))
        return 1; // burden is always 1 if the tx wasn't a emit

    auto const& pd = const_cast<ripple::STTx&>(tx).getField(sfEmitDetails).downcast<STObject>();

    if (!pd.isFieldPresent(sfEmitBurden)) {
        JLOG(j.trace())
            << "Hook found sfEmitDetails but sfEmitBurden was not in the object? ... ignoring";
        return 1;
    }

    uint64_t burden = pd.getFieldU64(sfEmitBurden);
    burden &= ((1ULL << 63)-1); // wipe out the two high bits just in case somehow they are set
    hookCtx.burden = burden;
    return (int64_t)(burden);
}

int64_t hook_api::etxn_generation ( wasmer_instance_context_t * wasm_ctx )
{
    return hook_api::otxn_generation ( wasm_ctx ) + 1;
}

int64_t hook_api::otxn_generation ( wasmer_instance_context_t * wasm_ctx )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    // cache the result as it will not change for this hook execution
    if (hookCtx.generation)
        return hookCtx.generation;

    auto const& tx = applyCtx.tx;
    if (!tx.isFieldPresent(sfEmitDetails))
        return 1; // generation is always 1 if the tx wasn't a emit

    auto const& pd = const_cast<ripple::STTx&>(tx).getField(sfEmitDetails).downcast<STObject>();

    if (!pd.isFieldPresent(sfEmitGeneration)) {
        JLOG(j.trace())
            << "Hook found sfEmitDetails but sfEmitGeneration was not in the object? ... ignoring";
        return 1;
    }

    hookCtx.generation = pd.getFieldU32(sfEmitGeneration);
    // this overflow will never happen in the life of the ledger but deal with it anyway
    if (hookCtx.generation + 1 > hookCtx.generation)
        hookCtx.generation++;

    return hookCtx.generation;
}


int64_t hook_api::ledger_seq ( wasmer_instance_context_t * wasm_ctx )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    return applyCtx.app.getLedgerMaster().getValidLedgerIndex() + 1;
}

int64_t hook_api::slot_field_txt (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t write_ptr, uint32_t write_len,
        uint32_t field_id, uint32_t slot )
{
    return NOT_IMPLEMENTED; // RH TODO
}

int64_t hook_api::slot_field (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t write_ptr, uint32_t write_len,
        uint32_t field_id, uint32_t slot )
{
    return NOT_IMPLEMENTED; // RH TODO
}


int64_t hook_api::slot_id (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t slot )
{
    return NOT_IMPLEMENTED; // RH TODO
}

int64_t hook_api::slot_type (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t slot )
{
    return NOT_IMPLEMENTED; // RH TODO
}

int64_t hook_api::otxn_field_txt (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t write_ptr, uint32_t write_len,
        uint32_t field_id )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    if (NOT_IN_BOUNDS(write_ptr, write_len, memory_length))
        return OUT_OF_BOUNDS;

    auto const& tx = applyCtx.tx;

    SField const& fieldType = ripple::SField::getField( field_id );

    if (fieldType == sfInvalid)
        return INVALID_FIELD;

    if (tx.getFieldIndex(fieldType) == -1)
        return DOESNT_EXIST;

    auto const& field = const_cast<ripple::STTx&>(tx).getField(fieldType);

    std::string out = field.getText();

    if (out.size() > write_len)
        return TOO_SMALL;

    WRITE_WASM_MEMORY_AND_RETURN(
        write_ptr, write_len,
        out.data(), out.size(),
        memory, memory_length);

}


int64_t hook_api::otxn_field (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t write_ptr, uint32_t write_len,
        uint32_t field_id )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    if (NOT_IN_BOUNDS(write_ptr, write_len, memory_length))
        return OUT_OF_BOUNDS;

    auto const& tx = applyCtx.tx;

    SField const& fieldType = ripple::SField::getField( field_id );

    if (fieldType == sfInvalid)
        return -1;

    if (tx.getFieldIndex(fieldType) == -1)
        return -2;

    auto const& field = const_cast<ripple::STTx&>(tx).getField(fieldType);

    bool is_account = field.getSType() == STI_ACCOUNT; //RH TODO improve this hack

    Serializer s;
    field.add(s);

    if (s.getDataLength() - (is_account ? 1 : 0) > write_len)
        return TOO_SMALL;

    WRITE_WASM_MEMORY_AND_RETURN(
        write_ptr, write_len,
        s.getDataPtr() + (is_account ? 1 : 0), s.getDataLength() - (is_account ? 1 : 0),
        memory, memory_length);

}

int64_t hook_api::slot_clear (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t slot_id  )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    if (slot_id > hook_api::max_slots)
        return TOO_BIG;

    if (hookCtx.slot.find(slot_id) == hookCtx.slot.end())
        return DOESNT_EXIST;

    hookCtx.slot.erase(slot_id);
    hookCtx.slot_free.push(slot_id);

    return slot_id;
}

int64_t hook_api::slot_set (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t read_ptr, uint32_t read_len,
        uint32_t slot_type, int32_t slot_into )
{

    return NOT_IMPLEMENTED;
    /*
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    if (NOT_IN_BOUNDS(read_ptr, read_len, memory_length))
        return OUT_OF_BOUNDS;

    // check if we can emplace the object to a slot
    if (hookCtx.slot_counter > hook_api::max_slots && hookCtx.slot_free.size() == 0)
        return NO_FREE_SLOTS;

    if (slot_type != 0)
        return INVALID_ARGUMENT; // RH TODO, add more slot types, slot type 0 means "load by hash"

    // find the object
    uint256 hash;
    if (!hash.SetHexExact((const char*)(memory + read_ptr)))      // RH NOTE: if ever changed to allow whitespace do
                                                                // a bounds check
        return INVALID_ARGUMENT;

    std::cout << "looking for hash: " << hash << "\n";

    ripple::error_code_i ec { ripple::error_code_i::rpcUNKNOWN };
    std::shared_ptr<ripple::Transaction> hTx = applyCtx.app.getMasterTransaction().fetch(hash, ec);
    if (!hTx)// || ec != ripple::error_code_i::rpcSUCCESS)
        return DOESNT_EXIST;

    std::cout << "(hash found)\n";
    int slot = -1;
    if (hookCtx.slot_free.size() > 0) {
        slot = hookCtx.slot_free.front();
        hookCtx.slot_free.pop();
    }
    else slot = hookCtx.slot_counter++;

    hookCtx.slot.emplace(slot, hTx);

    std::cout << "assigned to slot: " << slot << "\n";
    return slot;
    */

}

int32_t hook_api::_g (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t id, uint32_t maxitr )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    if (hookCtx.guard_map.find(id) == hookCtx.guard_map.end())
        hookCtx.guard_map[id] = 1;
    else
        hookCtx.guard_map[id]++;


    if (hookCtx.guard_map[id] > maxitr)
    {
        if (id > 0xFFFFU)
            fprintf(stderr, "MACRO GUARD VIOLATION src: %d macroline: %d, iterations=%d\n",
                    (id & 0xFFFFU), id >> 16, hookCtx.guard_map[id]);
        else
            fprintf(stderr, "GUARD VIOLATION id(line)=%d, iterations=%d\n", id, hookCtx.guard_map[id]);

        rollback(wasm_ctx, 0, 0, GUARD_VIOLATION);
    }

    return 1;
}

int64_t hook_api::trace_slot (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t slot )
{

    return NOT_IMPLEMENTED;
    /*
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    if (hookCtx.slot.find(slot) == hookCtx.slot.end())
        return DOESNT_EXIST;

    auto const& node = hookCtx.slot[slot];
    std::cout << "debug: object in slot " << slot << ":\n" << hookCtx.slot[slot] << "\n";

    return slot;
    */
}

int64_t hook_api::emit (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t read_ptr, uint32_t read_len )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    if (NOT_IN_BOUNDS(read_ptr, read_len, memory_length))
        return OUT_OF_BOUNDS;

    auto& app = hookCtx.applyCtx.app;

    if (hookCtx.expected_etxn_count < 0)
        return PREREQUISITE_NOT_MET;

    if (hookCtx.emitted_txn.size() >= hookCtx.expected_etxn_count)
        return TOO_MANY_EMITTED_TXN;

    ripple::Blob blob{memory + read_ptr, memory + read_ptr + read_len};

    printf("hook is emitting tx:-----\n");
    for (unsigned char c: blob)
    {
        printf("%02X", c);
    }
    printf("\n--------\n");

    SerialIter sitTrans(makeSlice(blob));
    std::shared_ptr<STTx const> stpTrans;
    try
    {
        stpTrans = std::make_shared<STTx const>(std::ref(sitTrans));
    }
    catch (std::exception& e)
    {
        std::cout << "EMISSION FAILURE 1 WHILE EMIT: " << e.what() << "\n";
        return EMISSION_FAILURE;
    }

    // check the emitted txn is valid
    /* Emitted TXN rules
     * 1. Sequence: 0
     * 2. PubSigningKey: 000000000000000
     * 3. sfEmitDetails present and valid
     * 4. No sfSignature
     * 5. LastLedgerSeq > current ledger, > firstledgerseq
     * 6. FirstLedgerSeq > current ledger, if present
     * 7. Fee must be correctly high
     */

    // rule 1: sfSequence must be present and 0
    if (!stpTrans->isFieldPresent(sfSequence) || stpTrans->getFieldU32(sfSequence) != 0)
    {
        std::cout << "EMISSION FAILURE, sfSequence missing or non-zero.\n";
        return EMISSION_FAILURE;
    }

    // rule 2: sfSigningPubKey must be present and 00...00
    if (!stpTrans->isFieldPresent(sfSigningPubKey))
    {
        std::cout << "EMISSION FAILURE, sfSigningPubKey missing.\n";
        return EMISSION_FAILURE;
    }

    auto const pk = stpTrans->getSigningPubKey();
    if (pk.size() != 33)
    {
        std::cout << "EMISSION FAILURE, sfSigningPubKey present but wrong size, expecting 33 bytes.\n";
        return EMISSION_FAILURE;
    }

    for (int i = 0; i < 33; ++i)
        if (pk[i] != 0)
        {
            std::cout << "EMISSION FAILURE, sfSigningPubKey present but non-zero.\n";
            return EMISSION_FAILURE;
        }

    // rule 3: sfEmitDetails must be present and valid
    if (!stpTrans->isFieldPresent(sfEmitDetails))
    {
        std::cout << "EMISSION FAILURE, sfEmitDetails missing.\n";
        return EMISSION_FAILURE;
    }

    auto const& emitDetails =
        const_cast<ripple::STTx&>(*stpTrans).getField(sfEmitDetails).downcast<STObject>();

    if (!emitDetails.isFieldPresent(sfEmitGeneration) ||
        !emitDetails.isFieldPresent(sfEmitBurden) ||
        !emitDetails.isFieldPresent(sfEmitParentTxnID) ||
        !emitDetails.isFieldPresent(sfEmitNonce) ||
        !emitDetails.isFieldPresent(sfEmitCallback))
    {
        std::cout<< "EMISSION FAILURE, sfEmitDetails malformed.\n";
        return EMISSION_FAILURE;
    }

    uint32_t gen = emitDetails.getFieldU32(sfEmitGeneration);
    uint64_t bur = emitDetails.getFieldU64(sfEmitBurden);
    ripple::uint256 pTxnID = emitDetails.getFieldH256(sfEmitParentTxnID);
    ripple::uint256 nonce = emitDetails.getFieldH256(sfEmitNonce);
    auto callback = emitDetails.getAccountID(sfEmitCallback);

    uint32_t gen_proper = etxn_generation(wasm_ctx);

    if (gen != gen_proper)
    {
        std::cout << "EMISSION FAILURE, Generation provided in EmitDetails was not correct: " << gen
            << " should be " << gen_proper << "\n";
        return EMISSION_FAILURE;
    }

    if (bur != etxn_burden(wasm_ctx))
    {
        std::cout << "EMISSION FAILURE, Burden provided in EmitDetails was not correct\n";
        return EMISSION_FAILURE;
    }

    if (pTxnID != applyCtx.tx.getTransactionID())
    {
        std::cout << "EMISSION FAILURE, ParentTxnID provided in EmitDetails was not correct\n";
        return EMISSION_FAILURE;
    }

    if (hookCtx.nonce_used.find(nonce) == hookCtx.nonce_used.end())
    {
        std::cout << "EMISSION FAILURE, Nonce provided in EmitDetails was not generated by nonce\n";
        return EMISSION_FAILURE;
    }

    if (callback != hookCtx.account)
    {
        std::cout << "EMISSION FAILURE, Callback account must be the account of the emitting hook\n";
        return EMISSION_FAILURE;
    }

    // rule 4: sfSignature must be absent
    if (stpTrans->isFieldPresent(sfSignature))
    {
        std::cout << "EMISSION FAILURE, sfSignature is present but should not be.\n";
        return EMISSION_FAILURE;
    }

    // rule 5: LastLedgerSeq must be present and after current ledger
    // RH TODO: limit lastledgerseq, is this needed?

    uint32_t tx_lls = stpTrans->getFieldU32(sfLastLedgerSequence);
    uint32_t ledgerSeq = applyCtx.app.getLedgerMaster().getValidLedgerIndex() + 1;
    if (!stpTrans->isFieldPresent(sfLastLedgerSequence) || tx_lls < ledgerSeq + 1)
    {
        std::cout << "EMISSION FAILURE, sfLastLedgerSequence missing or invalid\n";
        return EMISSION_FAILURE;
    }

    // rule 6
    if (stpTrans->isFieldPresent(sfFirstLedgerSequence) &&
            stpTrans->getFieldU32(sfFirstLedgerSequence) > tx_lls)
    {
        std::cout << "EMISSION FAILURE FirstLedgerSequence > LastLedgerSequence\n";
        return EMISSION_FAILURE;
    }


    // rule 7 check the emitted txn pays the appropriate fee

    if (hookCtx.fee_base == 0)
        hookCtx.fee_base = etxn_fee_base(wasm_ctx, read_len);

    int64_t minfee = hookCtx.fee_base * hook_api::drops_per_byte * read_len;
    if (minfee < 0 || hookCtx.fee_base < 0)
    {
        std::cout << "EMISSION FAILURE fee could not be calculated\n";
        return EMISSION_FAILURE;
    }

    if (!stpTrans->isFieldPresent(sfFee))
    {
        std::cout << "EMISSION FAILURE Fee missing from emitted tx\n";
        return EMISSION_FAILURE;
    }

    int64_t fee = stpTrans->getFieldAmount(sfFee).xrp().drops();
    if (fee < minfee)
    {
        std::cout << "EMISSION FAILURE Fee on emitted txn is less than the minimum required fee\n";
        return EMISSION_FAILURE;
    }

    std::string reason;
    auto tpTrans = std::make_shared<Transaction>(stpTrans, reason, app);
    if (tpTrans->getStatus() != NEW)
    {
        std::cout << "EMISSION FAILURE 2 WHILE EMIT: tpTrans->getStatus() != NEW\n";
        return EMISSION_FAILURE;
    }

    hookCtx.emitted_txn.push(tpTrans);

    return read_len;
}

int64_t hook_api::hook_hash (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t write_ptr, uint32_t ptr_len )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    return NOT_IMPLEMENTED; // RH TODO implement
}

int64_t hook_api::hook_account (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t write_ptr, uint32_t ptr_len )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    if (NOT_IN_BOUNDS(write_ptr, 20, memory_length))
        return OUT_OF_BOUNDS;


    WRITE_WASM_MEMORY_AND_RETURN(
        write_ptr, 20,
        hookCtx.account.data(), 20,
        memory, memory_length);
}

int64_t hook_api::nonce (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t write_ptr, uint32_t write_len )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx, view on current stack

    if (write_len < 32)
        return TOO_SMALL;

    if (NOT_IN_BOUNDS(write_ptr, write_len, memory_length))
        return OUT_OF_BOUNDS;

    if (hookCtx.nonce_counter > hook_api::max_nonce)
        return TOO_MANY_NONCES;

    auto hash = ripple::sha512Half(
            ripple::HashPrefix::emitTxnNonce,
            view.info().seq,
            hookCtx.nonce_counter++,
            hookCtx.account
    );

    hookCtx.nonce_used[hash] = true;

    WRITE_WASM_MEMORY_AND_RETURN(
        write_ptr, 32,
        hash.data(), 32,
        memory, memory_length);

    return 32;
}

int64_t hook_api::etxn_reserve (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t count )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    if (hookCtx.expected_etxn_count > -1)
        return ALREADY_SET;

    if (count > hook_api::max_emit)
        return TOO_BIG;

    hookCtx.expected_etxn_count = count;
    return count;
}


int64_t hook_api::etxn_burden (
        wasmer_instance_context_t * wasm_ctx )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    if (hookCtx.expected_etxn_count <= -1)
        return PREREQUISITE_NOT_MET;

    uint64_t last_burden = (uint64_t)hook_api::otxn_burden(wasm_ctx); // always non-negative so cast is safe

    uint64_t burden = last_burden * hookCtx.expected_etxn_count;
    if (burden < last_burden) // this overflow will never happen but handle it anyway
        return FEE_TOO_LARGE;

    return burden;
}
// hookCtx.applyCtx.app

int64_t hook_api::_special (
        wasmer_instance_context_t* wasm_ctx,
        uint32_t api_no,
        uint32_t a, uint32_t b, uint32_t c,
        uint32_t d, uint32_t e, uint32_t f )
{
    return NOT_IMPLEMENTED; // RH TODO
}


int64_t hook_api::util_sha512h (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t write_ptr, uint32_t write_len,
        uint32_t read_ptr, uint32_t read_len )
{
    return NOT_IMPLEMENTED; // RH TODO
}


int64_t hook_api::util_raddr (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t write_ptr, uint32_t write_len,
        uint32_t read_ptr, uint32_t read_len )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    if (NOT_IN_BOUNDS(write_ptr, write_len, memory_length))
        return OUT_OF_BOUNDS;

    if (NOT_IN_BOUNDS(read_ptr, read_len, memory_length))
        return OUT_OF_BOUNDS;

    if (read_len != 20)
        return INVALID_ARGUMENT;

    std::string raddr = base58EncodeToken(TokenType::AccountID, memory + read_ptr, read_len);

    if (write_len < raddr.size())
        return TOO_SMALL;

    WRITE_WASM_MEMORY_AND_RETURN(
        write_ptr, write_len,
        raddr.c_str(), raddr.size(),
        memory, memory_length);
}

int64_t hook_api::util_accid (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t write_ptr, uint32_t write_len,
        uint32_t read_ptr, uint32_t read_len )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    if (NOT_IN_BOUNDS(write_ptr, write_len, memory_length))
        return OUT_OF_BOUNDS;

    if (NOT_IN_BOUNDS(read_ptr, read_len, memory_length))
        return OUT_OF_BOUNDS;

    if (write_len < 20)
        return TOO_SMALL;

    auto const result = ripple::decodeBase58Token( std::string( (const char*)(memory + read_ptr),
                (size_t)(read_len) ), TokenType::AccountID );
    if (result.empty() || result.size() < 21)
        return INVALID_ARGUMENT;


    WRITE_WASM_MEMORY_AND_RETURN(
        write_ptr, write_len,
        result.data() + 1, 20,
        memory, memory_length);
}

int64_t hook_api::util_verify (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t sread_ptr, uint32_t sread_len,
        uint32_t kread_ptr, uint32_t kread_len )
{
    return NOT_IMPLEMENTED; // RH TODO

}

int64_t hook_api::fee_base (
        wasmer_instance_context_t * wasm_ctx )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    return (int64_t)((double)(view.fees().base.drops()) * hook_api::fee_base_multiplier);
}

int64_t hook_api::etxn_fee_base (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t tx_byte_count )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    if (hookCtx.expected_etxn_count <= -1)
        return PREREQUISITE_NOT_MET;

    uint64_t base_fee = (uint64_t)hook_api::fee_base(wasm_ctx); // will always return non-negative

    int64_t burden = hook_api::etxn_burden(wasm_ctx);
    if (burden < 1)
        return FEE_TOO_LARGE;

    uint64_t fee = base_fee * burden;
    if (fee < burden || fee & (3 << 62)) // a second under flow to handle
        return FEE_TOO_LARGE;

    hookCtx.fee_base = fee;

    return fee * hook_api::drops_per_byte * tx_byte_count;
}

int64_t hook_api::etxn_details (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t write_ptr, uint32_t write_len )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    if (NOT_IN_BOUNDS(write_ptr, write_len, memory_length))
        return OUT_OF_BOUNDS;

    if (write_len < hook_api::etxn_details_size)
        return TOO_SMALL;

    if (hookCtx.expected_etxn_count <= -1)
        return PREREQUISITE_NOT_MET;

    uint32_t generation = (uint32_t)(hook_api::etxn_generation(wasm_ctx)); // always non-negative so cast is safe

    int64_t burden = hook_api::etxn_burden(wasm_ctx);
    if (burden < 1)
        return FEE_TOO_LARGE;

    unsigned char* out = memory + write_ptr;

    *out++ = 0xECU; // begin sfEmitDetails                            /* upto =   0 | size =  1 */
    *out++ = 0x20U; // sfEmitGeneration preamble                      /* upto =   1 | size =  6 */
    *out++ = 0x2BU; // preamble cont
    *out++ = ( generation >> 24 ) & 0xFFU;
    *out++ = ( generation >> 16 ) & 0xFFU;
    *out++ = ( generation >>  8 ) & 0xFFU;
    *out++ = ( generation >>  0 ) & 0xFFU;
    *out++ = 0x3C; // sfEmitBurden preamble                           /* upto =   7 | size =  9 */
    *out++ = ( burden >> 56 ) & 0xFFU;
    *out++ = ( burden >> 48 ) & 0xFFU;
    *out++ = ( burden >> 40 ) & 0xFFU;
    *out++ = ( burden >> 32 ) & 0xFFU;
    *out++ = ( burden >> 24 ) & 0xFFU;
    *out++ = ( burden >> 16 ) & 0xFFU;
    *out++ = ( burden >>  8 ) & 0xFFU;
    *out++ = ( burden >>  0 ) & 0xFFU;
    *out++ = 0x5A; // sfEmitParentTxnID preamble                      /* upto =  16 | size = 33 */
    if (hook_api::otxn_id(wasm_ctx, out - memory, 32) != 32)
        return INTERNAL_ERROR;
    out += 32;
    *out++ = 0x5B; // sfEmitNonce                                     /* upto =  49 | size = 33 */
    if (hook_api::nonce(wasm_ctx, out - memory, 32) != 32)
        return INTERNAL_ERROR;
    out += 32;
    *out++ = 0x89; // sfEmitCallback preamble                         /* upto =  82 | size = 22 */
    *out++ = 0x14; // preamble cont
    if (hook_api::hook_account(wasm_ctx, out - memory, 20) != 20)
        return INTERNAL_ERROR;
    out += 20;
    *out++ = 0xE1U; // end object (sfEmitDetails)                     /* upto = 104 | size =  1 */
                                                                      /* upto = 105 | --------- */
    printf("emitdetails size = %d\n", (out - memory - write_ptr));
    return 105;
}
