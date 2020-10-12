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

TER hook::apply(Blob hook, ApplyContext& applyCtx, const AccountID& account) {

    wasmer_instance_t *instance = NULL;


    if (wasmer_instantiate(&instance, hook.data(), hook.size(), imports, imports_count) != wasmer_result_t::WASMER_OK) {
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
        "hook",
        arguments,
        1,
        results,
        1
    ); 
    
    
    /*!= wasmer_result_t::WASMER_OK) {
        printf("hook() call failed\n");
        printWasmerError();
        return temMALFORMED; /// todo: [RH] should be a hook execution error code tecHOOK_ERROR?
    }*/

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


int64_t hook_api::output_dbg ( wasmer_instance_context_t * wasm_ctx, uint32_t ptr, uint32_t len ) {

    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx on current stack

    printf("HOOKAPI_output_dbg: `");
    if (len > 1024) len = 1024;
    for (int i = 0; i < len && i < memory_length; ++i)
        printf("%c", memory[ptr + i]);
    printf("`\n");
    return len;

}
int64_t hook_api::set_state ( wasmer_instance_context_t * wasm_ctx, uint32_t key_ptr, uint32_t data_ptr_in, uint32_t in_len ) {

    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    if (key_ptr + 32 > memory_length || data_ptr_in + hook::maxHookDataSize() > memory_length) {
        JLOG(j.trace())
            << "Hook tried to set_state using memory outside of the wasm instance limit";
        return OUT_OF_BOUNDS;
    }
    
    if (in_len == 0)
        return TOO_SMALL;

    auto const sle = view.peek(hookCtx.hookKeylet);
    if (!sle)
        return INTERNAL_ERROR;
    
    uint32_t maxSize = sle->getFieldU32(sfHookDataMaxSize); 
    if (in_len > maxSize)
        return TOO_BIG;
   
    ripple::uint256 key = ripple::uint256::fromVoid(memory + key_ptr);
    
    (*hookCtx.changedState)[key] =
        std::pair<bool, ripple::Blob> (true, 
                {memory + data_ptr_in,  memory + data_ptr_in + in_len});

    return in_len;

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

    // next write all output emittx
    // RH TODO ^
}



int64_t hook_api::get_state ( wasmer_instance_context_t * wasm_ctx, uint32_t key_ptr, uint32_t data_ptr_out, uint32_t out_len ) {

    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    if (key_ptr + out_len > memory_length) {
        JLOG(j.trace())
            << "Hook tried to get_state using memory outside of the wasm instance limit";
        return OUT_OF_BOUNDS;
    }
   
    ripple::uint256 key= ripple::uint256::fromVoid(memory + key_ptr);

    // first check if the requested state was previously cached this session
    const auto& cacheEntry = hookCtx.changedState->find(key);
    if (cacheEntry != hookCtx.changedState->end())
        WRITE_WASM_MEMORY_AND_RETURN(
            data_ptr_out, out_len,
            cacheEntry->second.second.data(), cacheEntry->second.second.size(),
            memory, memory_length);

    // cache miss look it up
    auto const sle = view.peek(hookCtx.hookKeylet);
    if (!sle)
        return INTERNAL_ERROR;

    auto hsSLE = view.peek(keylet::hook_state(hookCtx.account, key));
    if (!hsSLE)
        return DOESNT_EXIST;
    
    Blob b = hsSLE->getFieldVL(sfHookData);

    // it exists add it to cache and return it
    hookCtx.changedState->emplace(key, std::pair<bool, ripple::Blob>(false, b));

    WRITE_WASM_MEMORY_AND_RETURN(
        data_ptr_out, out_len,
        b.data(), b.size(),
        memory, memory_length);
}



int64_t hook_api::accept     ( wasmer_instance_context_t * wasm_ctx, int32_t error_code, uint32_t data_ptr_in, uint32_t in_len ) {
    return hook_api::_exit(wasm_ctx, error_code, data_ptr_in, in_len, hook_api::ExitType::ACCEPT);
}
int64_t hook_api::reject     ( wasmer_instance_context_t * wasm_ctx, int32_t error_code, uint32_t data_ptr_in, uint32_t in_len ) {
    return hook_api::_exit(wasm_ctx, error_code, data_ptr_in, in_len, hook_api::ExitType::REJECT);
}
int64_t hook_api::rollback   ( wasmer_instance_context_t * wasm_ctx, int32_t error_code, uint32_t data_ptr_in, uint32_t in_len ) {
    return hook_api::_exit(wasm_ctx, error_code, data_ptr_in, in_len, hook_api::ExitType::ROLLBACK);
}

int64_t hook_api::_exit ( wasmer_instance_context_t * wasm_ctx, int32_t error_code, uint32_t data_ptr_in, uint32_t in_len, hook_api::ExitType exitType ) {

    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    if (data_ptr_in) {
        if (NOT_IN_BOUNDS(data_ptr_in, in_len, memory_length)) {
            JLOG(j.trace())
                << "Hook tried to accept/reject/rollback but specified memory outside of the wasm instance limit when specifying a reason string";
            return OUT_OF_BOUNDS;
        }

        hookCtx.exitReason = std::string ( (const char*)(memory + data_ptr_in), (size_t)in_len  );
    }

    hookCtx.exitType = exitType;
    hookCtx.exitCode = error_code;

    wasmer_raise_runtime_error(0, 0);

    // unreachable
    return 0;

}


int64_t hook_api::get_txn_id ( 
        wasmer_instance_context_t * wasm_ctx,
        uint32_t data_ptr_out )
{

    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    auto const& txID = applyCtx.tx.getTransactionID();
    
    if (NOT_IN_BOUNDS(data_ptr_out, txID.size(), memory_length)) 
        return OUT_OF_BOUNDS;

    WRITE_WASM_MEMORY_AND_RETURN(
        data_ptr_out, txID.size(),
        txID.data(), txID.size(),
        memory, memory_length);
}

int64_t hook_api::get_txn_type ( wasmer_instance_context_t * wasm_ctx ) 
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    return applyCtx.tx.getTxnType();
}

int64_t hook_api::get_burden ( wasmer_instance_context_t * wasm_ctx ) 
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

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
    return (int64_t)(burden);

}

int64_t hook_api::get_generation ( wasmer_instance_context_t * wasm_ctx ) 
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    auto const& tx = applyCtx.tx;
    if (!tx.isFieldPresent(sfEmitDetails)) 
        return 1; // burden is always 1 if the tx wasn't a emit

    auto const& pd = const_cast<ripple::STTx&>(tx).getField(sfEmitDetails).downcast<STObject>();

    if (!pd.isFieldPresent(sfEmitGeneration)) {
        JLOG(j.trace())
            << "Hook found sfEmitDetails but sfEmitGeneration was not in the object? ... ignoring";
        return 1;
    }

    return pd.getFieldU32(sfEmitGeneration);
}


int64_t hook_api::get_ledger_seq ( wasmer_instance_context_t * wasm_ctx ) 
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    return applyCtx.app.getLedgerMaster().getValidLedgerIndex() + 1;
}

int64_t hook_api::get_txn_field (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t field_id,
        uint32_t data_ptr_out,
        uint32_t out_len ) 
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    if (NOT_IN_BOUNDS(data_ptr_out, out_len, memory_length)) 
        return OUT_OF_BOUNDS;
   
    auto const& tx = applyCtx.tx;

    SField const& fieldType = ripple::SField::getField( field_id );

    if (fieldType == sfInvalid)
        return -1;

    if (tx.getFieldIndex(fieldType) == -1)
        return -2;

    auto const& field = const_cast<ripple::STTx&>(tx).getField(fieldType);
    
    std::string out = field.getText();

    WRITE_WASM_MEMORY_AND_RETURN(
        data_ptr_out, out_len,
        out.data(), out.size(),
        memory, memory_length);    

}


int64_t hook_api::get_obj_by_hash (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t hash_ptr ) 
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    if (NOT_IN_BOUNDS(hash_ptr, 64, memory_length)) 
        return OUT_OF_BOUNDS;

    // check if we can emplace the object to a slot
    if (hookCtx.slot_counter > 255 && hookCtx.slot_free.size() == 0)
        return NO_FREE_SLOTS;

    // find the object
    
    uint256 hash;
    if (!hash.SetHexExact((const char*)(memory + hash_ptr)))   // RH NOTE: if ever changed to allow whitespace do a bounds check
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
}


int64_t hook_api::output_dbg_obj (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t slot )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    if (hookCtx.slot.find(slot) == hookCtx.slot.end())
        return DOESNT_EXIST;
    auto const& node = hookCtx.slot[slot];

    std::cout << "debug: object in slot " << slot << ":\n" << hookCtx.slot[slot] << "\n";
/*        "\thash: " << node->getHash() << "\n" <<
        "\ttype: " << node->getType() << "\n" <<
        "\tdata: \n";
    auto const& data = node->getData();
    for (uint8_t c: data)
        std::cout << std::setfill('0') << std::setw(2) << std::hex << c;
*/
    return 1;
} 

int64_t hook_api::emit_txn (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t tx_ptr_in,
        uint32_t in_len )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    if (NOT_IN_BOUNDS(tx_ptr_in, in_len, memory_length))
        return OUT_OF_BOUNDS;
    auto & app = hookCtx.applyCtx.app;



    printf("received tx from hook:----\n");
    for (int i = 0; i < in_len; ++i)
        printf("%02X", *(memory + tx_ptr_in + i));
    printf("------\n");


    auto & netOps = app.getOPs();
    ripple::Blob blob{memory + tx_ptr_in, memory + tx_ptr_in + in_len};

    printf("emitting tx:-----\n");
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

    std::string reason;
    auto tpTrans = std::make_shared<Transaction>(stpTrans, reason, app);
    if (tpTrans->getStatus() != NEW)
    {
        std::cout << "EMISSION FAILURE 2 WHILE EMIT: tpTrans->getStatus() != NEW\n";
        return EMISSION_FAILURE;
    }

    try
    {
        // submit to network
        netOps.processTransaction(
            tpTrans, false, false, true, NetworkOPs::FailHard::yes);
    }
    catch (std::exception& e)
    {
        std::cout << "EMISSION FAILURE 3 WHILE EMIT: " << e.what() << "\n";
        return EMISSION_FAILURE;
    }    

    return in_len;
}

int64_t hook_api::get_hook_account (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t ptr_out )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    if (NOT_IN_BOUNDS(ptr_out, 20, memory_length))
        return OUT_OF_BOUNDS;


    WRITE_WASM_MEMORY_AND_RETURN(
        ptr_out, 20,
        hookCtx.account.data(), 20,
        memory, memory_length);
}

int64_t hook_api::get_nonce ( 
        wasmer_instance_context_t * wasm_ctx,
        uint32_t ptr_out)
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx, view on current stack
    if (NOT_IN_BOUNDS(ptr_out, 32, memory_length))
        return OUT_OF_BOUNDS;

    auto hash = ripple::sha512Half( 
            ripple::HashPrefix::emitTxnNonce,
            view.info().seq,
            hookCtx.nonce_counter++,
            hookCtx.account
    );
             
    WRITE_WASM_MEMORY_AND_RETURN(
        ptr_out, 32,
        hash.data(), 32,
        memory, memory_length);

    return 32;
}

int64_t hook_api::set_emit_count (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t c )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    if (hookCtx.expected_emit_count > -1)
        return ALREADY_SET;

    if (c > 256) // RH TODO make this a configurable value
        return TOO_BIG;

    hookCtx.expected_emit_count = c;
    return c;
}


int64_t hook_api::get_emit_burden ( 
        wasmer_instance_context_t * wasm_ctx )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    if (hookCtx.expected_emit_count <= -1)
        return PREREQUISITE_NOT_MET;

    uint64_t last_burden = (uint64_t)hook_api::get_burden(wasm_ctx); // will always return a non-negative so cast is safe
    
    uint64_t burden = last_burden * hookCtx.expected_emit_count;
    if (burden < last_burden) // this overflow will never happen but handle it anyway
        return FEE_TOO_LARGE; 

    return burden;
}
// hookCtx.applyCtx.app
//
//

int64_t hook_api::get_fee_base ( 
        wasmer_instance_context_t * wasm_ctx )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    {
        auto const view = applyCtx.app.openLedger().current();
        if (!view)
           return INTERNAL_ERROR;
        auto const metrics = applyCtx.app.getTxQ().getMetrics(*view);
        auto const baseFee = view->fees().base; 

        return std::max ({   // for the best chance of inclusion in the ledger we pick the largest of these metrics
            toDrops(metrics.medFeeLevel, baseFee).second.drops(),
            toDrops(metrics.minProcessingFeeLevel, baseFee).second.drops(),
            toDrops(metrics.openLedgerFeeLevel, baseFee).second.drops()
            });
    }

}
int64_t hook_api::get_emit_fee_base ( 
        wasmer_instance_context_t * wasm_ctx,
        uint32_t emit_tx_byte_count )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    if (hookCtx.expected_emit_count <= -1)
        return PREREQUISITE_NOT_MET;

    uint64_t base_fee = (uint64_t)hook_api::get_fee_base(wasm_ctx); // will always return non-negative

    int64_t burden = hook_api::get_emit_burden(wasm_ctx);
    if (burden < 1)
        return FEE_TOO_LARGE;

    uint64_t fee = base_fee * burden;
    if (fee < burden || fee & (3 << 62)) // a second under flow to handle
        return FEE_TOO_LARGE; 

    // RH TODO calculate some fee adjustment based on the emit_tx_byte_count

    return fee;
}

int64_t hook_api::get_emit_details ( 
        wasmer_instance_context_t * wasm_ctx,
        uint32_t ptr_out,
        uint32_t out_len )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    if (NOT_IN_BOUNDS(ptr_out, out_len, memory_length))
        return OUT_OF_BOUNDS;

    if (out_len < hook_api::emit_details_size)
        return TOO_SMALL;

    if (hookCtx.expected_emit_count <= -1)
        return PREREQUISITE_NOT_MET;

    uint32_t generation = (uint32_t)(hook_api::get_generation(wasm_ctx)); // will always return non-negative so cast is safe
    if (generation + 1 > generation) generation++; // this overflow will never happen in the life of the ledger but deal with it anyway

    int64_t burden = hook_api::get_emit_burden(wasm_ctx);
    if (burden < 1)
        return FEE_TOO_LARGE;

    unsigned char* out = memory + ptr_out;

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
    if (hook_api::get_txn_id(wasm_ctx, out - memory) != 32)
        return INTERNAL_ERROR;
    out += 32;
    *out++ = 0x5B; // sfEmitNonce                                     /* upto =  49 | size = 33 */
    if (hook_api::get_nonce(wasm_ctx, out - memory) != 32)
        return INTERNAL_ERROR;
    out += 32;
    *out++ = 0x89; // sfEmitCallback preamble                         /* upto =  82 | size = 22 */
    *out++ = 0x14; // preamble cont
    if (hook_api::get_hook_account(wasm_ctx, out - memory) != 20)
        return INTERNAL_ERROR;
    out += 20;
    *out++ = 0xE1U; // end object (sfEmitDetails)                     /* upto = 104 | size =  1 */
                                                                      /* upto = 105 | --------- */
    printf("emitdetails size = %d\n", (out - memory - ptr_out));
    return 105; 
}
