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
#include <memory>
#include <string>
#include <optional>

using namespace ripple;

/* returns true iff every even char is ascii and every odd char is 00
 * only a hueristic, may be inaccurate in edgecases */
inline bool is_UTF16LE(const uint8_t* buffer, size_t len)
{
    if (len % 2 != 0 || len == 0)
        return false;

    for (int i = 0; i < len; i+=2)
        if (buffer[i + 0] == 0 || buffer[i + 1] != 0)
            return false;

    return true;
}


// Called by Transactor.cpp to determine if a transaction type can trigger a given hook...
// The HookOn field in the SetHook transaction determines which transaction types (tt's) trigger the hook.
// Every bit except ttHookSet is active low, so for example ttESCROW_FINISH = 2, so if the 2nd bit (counting from 0)
// from the right is 0 then the hook will trigger on ESCROW_FINISH. If it is 1 then ESCROW_FINISH will not trigger
// the hook. However ttHOOK_SET = 22 is active high, so by default (HookOn == 0) ttHOOK_SET is not triggered by
// transactions. If you wish to set a hook that has control over ttHOOK_SET then set bit 1U<<22.
bool hook::canHook(ripple::TxType txType, uint64_t hookOn) {
    // invert ttHOOK_SET bit
    hookOn ^= (1ULL << ttHOOK_SET);
    // invert entire field
    hookOn ^= 0xFFFFFFFFFFFFFFFFULL;
    return (hookOn >> txType) & 1;
}


// Update HookState ledger objects for the hook... only called after accept() or reject()
TER
hook::setHookState(
    HookResult& hookResult,
    ripple::ApplyContext& applyCtx,
    Keylet const& hookStateKeylet,
    ripple::uint256 key,
    Slice& data
){

    auto& view = applyCtx.view();
    auto j = applyCtx.app.journal("View");
    auto const sle = view.peek(hookResult.accountKeylet);
    if (!sle)
        return tefINTERNAL;

    auto const hook = view.peek(hookResult.hookKeylet);
    if (!hook) {
        JLOG(j.warn()) <<
            "Attempted to set a hook state for a hook that doesnt exist " << toBase58(hookResult.account);
        return tefINTERNAL;
    }

    uint32_t hookDataMax = hook->getFieldU32(sfHookStateDataMaxSize);

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
        if (!view.dirRemove(hookResult.ownerDirKeylet, hint, hookStateKeylet.key, false))
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
    newHookState->setFieldVL(sfHookStateData, data);
    newHookState->setFieldH256(sfHookStateKey, key);

    if (!oldHookState) {
        // Add the hook to the account's directory if it wasn't there already
        auto const page = dirAdd(
            view,
            hookResult.ownerDirKeylet,
            hookStateKeylet.key,
            false,
            describeOwnerDir(hookResult.account),
            j);

        JLOG(j.trace()) << "Create/update hook state for account " << toBase58(hookResult.account)
                     << ": " << (page ? "success" : "failure");

        if (!page)
            return tecDIR_FULL;

        newHookState->setFieldU64(sfOwnerNode, *page);

    }

    return tesSUCCESS;
}

void hook::printWasmerError(beast::Journal::Stream const& x)
{
  int error_len = wasmer_last_error_length();
  char *error_str = (char*)malloc(error_len);
  wasmer_last_error_message(error_str, error_len);
  JLOG(x) << error_str << "\n";
  free(error_str);
}

//#define HOOK_CACHING  // RH NOTE: WASMER USES GLOBAL STATE IN ITS C API WHICH BREAKS CACHING SO CACHING IS OFF
                        //          WASMER RUNTIME TO BE PATCHED OR SCRAPPED... to be decided

hook::HookResult
    hook::apply(
        ripple::uint256 hookSetTxnID, /* this is the txid of the sethook, used for caching */
        Blob hook, ApplyContext& applyCtx,
        const AccountID& account,
        bool callback = false)
{

    HookContext hookCtx = 
    {
        .applyCtx = applyCtx,
        // we will return this context object (RVO / move constructed)
        .result = {
            .accountKeylet = keylet::account(account),
            .ownerDirKeylet = keylet::ownerDir(account),
            .hookKeylet = keylet::hook(account),
            .account = account,
            .changedState =
                std::make_shared<std::map<ripple::uint256, std::pair<bool, ripple::Blob>>>(),
            .exitType = hook_api::ExitType::ROLLBACK, // default is to rollback unless hook calls accept()
            .exitReason = std::string("<not set by hook>"),
            .exitCode = -1
        }
    };
    /*
    
    struct HookResult
    {
        ripple::AccountID account;
        std::queue<std::shared_ptr<ripple::Transaction>> emittedTxn; // etx stored here until accept/rollback
        // uint256 key -> [ has_been_modified, current_state ]
        std::shared_ptr<std::map<ripple::uint256, std::pair<bool, ripple::Blob>>> changedState;
        hook_api::ExitType exitType = hook_api::ExitType::ROLLBACK;
        std::string exitReason {""};
        int64_t exitCode {-1};
    };
*/
    auto const& j = applyCtx.app.journal("View");
    wasmer_instance_t *instance = NULL;

#ifdef HOOK_CACHING
    {
        auto const& cacheLookup = hook_cache.find(account);
        bool needsDestruction = false;
        if (cacheLookup  == hook_cache.end() ||
                (cacheLookup->second.first != hookSetTxnID && (needsDestruction = true)))
        {
            if (needsDestruction)
            {
                JLOG(j.trace()) << "Hook Apply: Destroying wasmer instance for " << account <<
                   " Instance Ptr: " << cacheLookup->second.second << "\n";
                wasmer_instance_destroy(cacheLookup->second.second);
                hook_cache.erase(account);
            }
#endif
            JLOG(j.trace()) << "Hook Apply: Creating wasmer instance for " << account << "\n";
            if (wasmer_instantiate(&instance, hook.data(), hook.size(), imports, imports_count) != 
                    wasmer_result_t::WASMER_OK)
            {
                printWasmerError(j.warn());
                JLOG(j.warn()) << "Hook Apply: Hook was malformed for " << account << "\n";
                hookCtx.result.exitType = hook_api::ExitType::WASM_ERROR;
                return hookCtx.result;
            }
#ifdef HOOK_CACHING
            hook_cache[account] = std::make_pair(hookSetTxnID, instance);
        } else
        {
            instance = hook_cache[account].second;
            JLOG(j.trace()) << "Hook Apply: Loading cached wasmer instance for " << account <<
                " Instance Ptr " << instance << "\n";
        }
    }
#endif


    wasmer_instance_context_data_set ( instance, &hookCtx );
    DBG_PRINTF("Set HookContext: %lx\n", (void*)&hookCtx);

    wasmer_value_t arguments[] = { { .tag = wasmer_value_tag::WASM_I64, .value = {.I64 = 0 } } };
    wasmer_value_t results[] = { { .tag = wasmer_value_tag::WASM_I64, .value = {.I64 = 0 } } };

    wasmer_instance_call(
        instance,
        (!callback ? "hook" : "cbak"),
        arguments,
        1,
        results,
        1
    ); // we don't check return value because all accept/reject/rollback will fire a non-ok message

    JLOG(j.trace()) <<
        "Hook Apply: Exited with " <<
            ( hookCtx.result.exitType == hook_api::ExitType::ROLLBACK ? "ROLLBACK" :
            ( hookCtx.result.exitType == hook_api::ExitType::ACCEPT ? "ACCEPT" : "REJECT" ) ) <<
        ", Reason: '" <<  hookCtx.result.exitReason.c_str() << "', Exit Code: " << hookCtx.result.exitCode <<
        ", Ledger Seq: " << hookCtx.applyCtx.view().info().seq << "\n";

    if (hookCtx.result.exitType != hook_api::ExitType::ROLLBACK && callback) // callback auto-commits on non-rollback
        commitChangesToLedger(hookCtx.result, applyCtx);

    // RH TODO possible memory leak here, destroy the imports, instance using a smart pointer?

#ifndef HOOK_CACHING
    JLOG(j.trace()) <<
        "Hook Apply: Destroying instance for " << account << " Instance Ptr: " << instance << "\n";
    wasmer_instance_destroy(instance);
#endif
    return hookCtx.result;
}



/* If XRPLD is running with trace log level hooks may produce debugging output to the trace log
 * specifying both a string and an integer to output */
int64_t hook_api::trace_num (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t read_ptr, uint32_t read_len, int64_t number )
{

    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx on current stack
    if (NOT_IN_BOUNDS(read_ptr, read_len, memory_length))
        return OUT_OF_BOUNDS;
    if (read_len > 1024) read_len = 1024;
    if (!j.trace())
        return read_len;
    j.trace() << "HOOK::TRACE(num): " << std::string_view((const char*)(memory + read_ptr), (size_t)read_len) << 
        ": " << number << "\n";
    return read_len;

}


/* If XRPLD is running with trace log level hooks may produce debugging output to the trace log
 * specifying as_hex dumps memory as hex */
int64_t hook_api::trace (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t read_ptr, uint32_t read_len, uint32_t as_hex )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx on current stack
    if (NOT_IN_BOUNDS(read_ptr, read_len, memory_length))
        return OUT_OF_BOUNDS;
    if (read_len > 1024) read_len = 1024; // max trace is 1 kib
    if (!j.trace())
        return read_len;
    if (as_hex)
    {
        uint8_t output[2048];
        for (int i = 0; i < read_len && i < memory_length; ++i)
        {
            unsigned char high = (memory[read_ptr + i] >> 4) & 0xF;
            unsigned char low  = (memory[read_ptr + i] & 0xF);
            high += ( high < 10U ? '0' : 'A' - 10 );
            low  += ( low  < 10U ? '0' : 'A' - 10 );
            output[i*2 + 0] = high;
            output[i*2 + 1] = low;
        }
        j.trace() << "HOOK::TRACE(hex): '" << std::string_view((const char*)output, (size_t)(read_len*2)) << "'\n";
    }
    else if (is_UTF16LE(memory + read_ptr, read_len))
    {
        uint8_t output[1024];
        int len = read_len / 2; //is_UTF16LE will only return true if read_len is even
        for (int i = 0; i < len; ++i)
            output[i] = memory[read_ptr + i * 2];
        j.trace() << "HOOK::TRACE(txt): '" << std::string_view((const char*)output, (size_t)(len)) << "'\n";
    }
    else
    {
        
        j.trace() << "HOOK::TRACE(txt): '" << std::string_view((const char*)(memory + read_ptr), (size_t)read_len)
            << "'\n";
    }
    return read_len;
}


// zero pad on the left a key to bring it up to 32 bytes
std::optional<ripple::uint256>
inline
make_state_key(
        std::string_view source)
{

    size_t source_len = source.size();

    if (source_len > 32 || source_len < 1)
        return std::nullopt;

    unsigned char key_buffer[32];
    int i = 0;
    int pad = 32 - source_len;

    // zero pad on the left
    for (; i < pad; ++i)
        key_buffer[i] = 0;

    const char* data = source.data();

    for (; i < 32; ++i)
        key_buffer[i] = data[i - pad];

    return ripple::uint256::fromVoid(key_buffer);
}

// update or create a hook state object
// read_ptr = data to set, kread_ptr = key
// RH NOTE passing 0 size causes a delete operation which is as-intended
// RH TODO: check reserve
int64_t hook_api::state_set (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t read_ptr, uint32_t read_len,
        uint32_t kread_ptr, uint32_t kread_len )
{

    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    if (kread_ptr + 32 > memory_length || read_ptr + hook::maxHookStateDataSize() > memory_length) {
        JLOG(j.trace())
            << "Hook tried to state_set using memory outside of the wasm instance limit";
        return OUT_OF_BOUNDS;
    }

    if (kread_len > 32)
        return TOO_BIG;

    if (kread_len < 1)
        return TOO_SMALL;


    auto const sle = view.peek(hookCtx.result.hookKeylet);
    if (!sle)
        return INTERNAL_ERROR;

    uint32_t maxSize = sle->getFieldU32(sfHookStateDataMaxSize);
    if (read_len > maxSize)
        return TOO_BIG;

    auto const key =
        make_state_key( std::string_view { (const char*)(memory + kread_ptr), (size_t)kread_len } );

    (*hookCtx.result.changedState)[*key] =
        std::pair<bool, ripple::Blob> (true,
                {memory + read_ptr,  memory + read_ptr + read_len});

    return read_len;
}


void hook::commitChangesToLedger(
        hook::HookResult& hookResult,
        ripple::ApplyContext& applyCtx)
{

    // first write all changes to state

    for (const auto& cacheEntry : *(hookResult.changedState)) {
        bool is_modified = cacheEntry.second.first;
        const auto& key = cacheEntry.first;
        const auto& blob = cacheEntry.second.second;
        if (is_modified) {
            // this entry isn't just cached, it was actually modified
            auto HSKeylet = keylet::hook_state(hookResult.account, key);
            auto slice = Slice(blob.data(), blob.size());
            setHookState(hookResult, applyCtx, HSKeylet, key, slice); 
            // ^ should not fail... checks were done before map insert
        }
    }

    DBG_PRINTF("emitted txn count: %d\n", hookResult.emittedTxn.size());

    auto const& j = applyCtx.app.journal("View");
    auto & netOps = applyCtx.app.getOPs();
    for (; hookResult.emittedTxn.size() > 0; hookResult.emittedTxn.pop())
    {
        auto& tpTrans = hookResult.emittedTxn.front();
        JLOG(j.trace()) << "Hook emitted tx: " << tpTrans->getID() << "\n";
        try
        {
            netOps.processTransaction(
                tpTrans, false, false, true, NetworkOPs::FailHard::yes);
        }
        catch (std::exception& e)
        {
            JLOG(j.warn()) << "Hook emitted tx failed to process: " << e.what() << "\n";
        }
    }

}

/* Retrieve the state into write_ptr identified by the key in kread_ptr */
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

/* This api actually serves both local and foreign state requests
 * feeding aread_ptr = 0 and aread_len = 0 will cause it to read local */
int64_t hook_api::state_foreign (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t write_ptr, uint32_t write_len,
        uint32_t kread_ptr, uint32_t kread_len,
        uint32_t aread_ptr, uint32_t aread_len )
{

    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    bool is_foreign = false;
    if (aread_ptr == 0)
    {
        // valid arguments, local state
    } else if (aread_ptr > 0)
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

    if (is_foreign && aread_len != 20)
        return INVALID_ACCOUNT;


    auto const key =
        make_state_key( std::string_view { (const char*)(memory + kread_ptr), (size_t)kread_len } );

    if (!key)
        return INVALID_ARGUMENT;

    // first check if the requested state was previously cached this session
    if (!is_foreign) // we only cache local
    {
        const auto& cacheEntry = hookCtx.result.changedState->find(*key);
        if (cacheEntry != hookCtx.result.changedState->end())
        {
            if (write_ptr == 0)
                return data_as_int64(cacheEntry->second.second.data(), cacheEntry->second.second.size());

            if (cacheEntry->second.second.size() > write_len)
                return TOO_SMALL;

            WRITE_WASM_MEMORY_AND_RETURN(
                write_ptr, write_len,
                cacheEntry->second.second.data(), cacheEntry->second.second.size(),
                memory, memory_length);
        }
    }

    // cache miss look it up
    auto const sle = view.peek(hookCtx.result.hookKeylet);
    if (!sle)
        return INTERNAL_ERROR;

    auto hsSLE = view.peek(keylet::hook_state(
                (is_foreign ? AccountID::fromVoid(memory + aread_ptr) : hookCtx.result.account), *key));
    if (!hsSLE)
        return DOESNT_EXIST;

    Blob b = hsSLE->getFieldVL(sfHookStateData);

    // it exists add it to cache and return it

    if (!is_foreign)
        hookCtx.result.changedState->emplace(*key, std::pair<bool, ripple::Blob>(false, b));

    if (write_ptr == 0)
        return data_as_int64(b.data(), b.size());

    if (b.size() > write_len)
        return TOO_SMALL;

    WRITE_WASM_MEMORY_AND_RETURN(
        write_ptr, write_len,
        b.data(), b.size(),
        memory, memory_length);
}


// Cause the originating transaction to go through, save state changes and emit emitted tx, exit hook
int64_t hook_api::accept (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t read_ptr, uint32_t read_len,
        int32_t error_code )
{
    return hook_api::_exit(wasm_ctx, read_ptr, read_len, error_code, hook_api::ExitType::ACCEPT);
}

// Cause the originating transaction to be rejected, discard state changes and discard emitted tx, exit hook
int64_t hook_api::rollback (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t read_ptr, uint32_t read_len,
        int32_t error_code )
{
    return hook_api::_exit(wasm_ctx, read_ptr, read_len, error_code, hook_api::ExitType::ROLLBACK);
}

// called by the above three exit methods
int64_t hook_api::_exit (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t read_ptr, uint32_t read_len,
        int32_t error_code, hook_api::ExitType exitType )
{

    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    if (read_len > 1024) read_len = 1024;

    if (read_ptr) {
        if (NOT_IN_BOUNDS(read_ptr, read_len, memory_length)) {
            JLOG(j.trace())
                << "Hook tried to accept/reject/rollback but specified memory outside of the wasm instance " <<
                "limit when specifying a reason string\n";
            return OUT_OF_BOUNDS;
        }

        // assembly script and some other languages use utf16 for strings
        if (is_UTF16LE(read_ptr + memory, read_len))
        {
            uint8_t output[512];
            int len = read_len / 2; //is_UTF16LE will only return true if read_len is even
            for (int i = 0; i < len; ++i)
                output[i] = memory[read_ptr + i * 2];
            hookCtx.result.exitReason = std::string((const char*)(output), (size_t)len);
        } else
            hookCtx.result.exitReason = std::string((const char*)(memory + read_ptr), (size_t)read_len);
    }

    hookCtx.result.exitType = exitType;
    hookCtx.result.exitCode = error_code;

    wasmer_raise_runtime_error(0, 0);

    // unreachable
    return 0;

}


// Write the TxnID of the originating transaction into the write_ptr
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

// Return the tt (Transaction Type) numeric code of the originating transaction
int64_t hook_api::otxn_type ( wasmer_instance_context_t * wasm_ctx )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    return applyCtx.tx.getTxnType();
}

// Return the burden of the originating transaction... this will be 1 unless the originating transaction
// was itself an emitted transaction from a previous hook invocation
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

// Return the generation of the originating transaction... this will be 1 unless the originating transaction
// was itself an emitted transaction from a previous hook invocation
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

// Return the generation of a hypothetically emitted transaction from this hook
int64_t hook_api::etxn_generation ( wasmer_instance_context_t * wasm_ctx )
{
    return hook_api::otxn_generation ( wasm_ctx ) + 1;
}


// Return the current ledger sequence number
int64_t hook_api::ledger_seq ( wasmer_instance_context_t * wasm_ctx )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    return applyCtx.app.getLedgerMaster().getValidLedgerIndex() + 1;
}


// Dump a field in 'full text' form into the hook's memory
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

// Dump a field from the originating transaction into the hook's memory
int64_t hook_api::otxn_field (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t write_ptr, uint32_t write_len,
        uint32_t field_id )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    if (write_ptr != 0 && NOT_IN_BOUNDS(write_ptr, write_len, memory_length))
        return OUT_OF_BOUNDS;

    auto const& tx = applyCtx.tx;

    SField const& fieldType = ripple::SField::getField( field_id );

    if (fieldType == sfInvalid)
        return INVALID_FIELD;

    if (tx.getFieldIndex(fieldType) == -1)
        return DOESNT_EXIST;

    auto const& field = const_cast<ripple::STTx&>(tx).getField(fieldType);

    bool is_account = field.getSType() == STI_ACCOUNT; //RH TODO improve this hack

    Serializer s;
    field.add(s);

    if (write_ptr == 0)
        return data_as_int64(s.getDataPtr(), s.getDataLength());

    if (s.getDataLength() - (is_account ? 1 : 0) > write_len)
        return TOO_SMALL;

    WRITE_WASM_MEMORY_AND_RETURN(
        write_ptr, write_len,
        s.getDataPtr() + (is_account ? 1 : 0), s.getDataLength() - (is_account ? 1 : 0),
        memory, memory_length);
}


// RH NOTE: slot system is not yet implemented, but planned feature for prod
int64_t hook_api::slot_clear (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t slot_id  )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    return NOT_IMPLEMENTED;
    /*
    if (slot_id > hook_api::max_slots)
        return TOO_BIG;

    if (hookCtx.slot.find(slot_id) == hookCtx.slot.end())
        return DOESNT_EXIST;

    hookCtx.slot.erase(slot_id);
    hookCtx.slot_free.push(slot_id);

    return slot_id;
    */
}

// RH NOTE: slot system is not yet implemented, but planned feature for prod
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

// Slots unimplemented
int64_t hook_api::slot_field_txt (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t write_ptr, uint32_t write_len,
        uint32_t field_id, uint32_t slot )
{
    return NOT_IMPLEMENTED; // RH TODO
}

// Slots unimplemented
int64_t hook_api::slot_field (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t write_ptr, uint32_t write_len,
        uint32_t field_id, uint32_t slot )
{
    return NOT_IMPLEMENTED; // RH TODO
}


// Slots unimplemented
int64_t hook_api::slot_id (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t slot )
{
    return NOT_IMPLEMENTED; // RH TODO
}

// Slots unimplemented
int64_t hook_api::slot_type (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t slot )
{
    return NOT_IMPLEMENTED; // RH TODO
}

// Slots unimplemented
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


/* Emit a transaction from this hook. Transaction must be in STObject form, fully formed and valid.
 * XRPLD does not modify transactions it only checks them for validity. */
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

    if (hookCtx.result.emittedTxn.size() >= hookCtx.expected_etxn_count)
        return TOO_MANY_EMITTED_TXN;

    ripple::Blob blob{memory + read_ptr, memory + read_ptr + read_len};

    DBG_PRINTF("hook is emitting tx:-----\n");
    for (unsigned char c: blob)
        DBG_PRINTF("%02X", c);
    DBG_PRINTF("\n--------\n");

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
        JLOG(j.trace()) << "Hook emission failure: sfSequence missing or non-zero.\n";
        return EMISSION_FAILURE;
    }

    // rule 2: sfSigningPubKey must be present and 00...00
    if (!stpTrans->isFieldPresent(sfSigningPubKey))
    {
        JLOG(j.trace()) << "Hook emission failure: sfSigningPubKey missing.\n";
        return EMISSION_FAILURE;
    }

    auto const pk = stpTrans->getSigningPubKey();
    if (pk.size() != 33)
    {
        JLOG(j.trace()) << "Hook emission failure: sfSigningPubKey present but wrong size, expecting 33 bytes.\n";
        return EMISSION_FAILURE;
    }

    for (int i = 0; i < 33; ++i)
        if (pk[i] != 0)
        {
            JLOG(j.trace()) << "Hook emission failure: sfSigningPubKey present but non-zero.\n";
            return EMISSION_FAILURE;
        }

    // rule 3: sfEmitDetails must be present and valid
    if (!stpTrans->isFieldPresent(sfEmitDetails))
    {
        JLOG(j.trace()) << "Hook emission failure: sfEmitDetails missing.\n";
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
        JLOG(j.trace()) << "Hook emission failure: sfEmitDetails malformed.\n";
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
        JLOG(j.trace()) << "Hook emission failure: Generation provided in EmitDetails was not correct: " << gen
            << " should be " << gen_proper << "\n";
        return EMISSION_FAILURE;
    }

    if (bur != etxn_burden(wasm_ctx))
    {
        JLOG(j.trace()) << "Hook emission failure: Burden provided in EmitDetails was not correct\n";
        return EMISSION_FAILURE;
    }

    if (pTxnID != applyCtx.tx.getTransactionID())
    {
        JLOG(j.trace()) << "Hook emission failure: ParentTxnID provided in EmitDetails was not correct\n";
        return EMISSION_FAILURE;
    }

    if (hookCtx.nonce_used.find(nonce) == hookCtx.nonce_used.end())
    {
        JLOG(j.trace()) << "Hook emission failure: Nonce provided in EmitDetails was not generated by nonce\n";
        return EMISSION_FAILURE;
    }

    if (callback != hookCtx.result.account)
    {
        JLOG(j.trace()) << "Hook emission failure: Callback account must be the account of the emitting hook\n";
        return EMISSION_FAILURE;
    }

    // rule 4: sfSignature must be absent
    if (stpTrans->isFieldPresent(sfSignature))
    {
        JLOG(j.trace()) << "Hook emission failure: sfSignature is present but should not be.\n";
        return EMISSION_FAILURE;
    }

    // rule 5: LastLedgerSeq must be present and after current ledger
    // RH TODO: limit lastledgerseq, is this needed?

    uint32_t tx_lls = stpTrans->getFieldU32(sfLastLedgerSequence);
    uint32_t ledgerSeq = applyCtx.app.getLedgerMaster().getValidLedgerIndex() + 1;
    if (!stpTrans->isFieldPresent(sfLastLedgerSequence) || tx_lls < ledgerSeq + 1)
    {
        JLOG(j.trace()) << "Hook emission failure: sfLastLedgerSequence missing or invalid\n";
        return EMISSION_FAILURE;
    }

    // rule 6
    if (stpTrans->isFieldPresent(sfFirstLedgerSequence) &&
            stpTrans->getFieldU32(sfFirstLedgerSequence) > tx_lls)
    {
        JLOG(j.trace()) << "Hook emission failure: FirstLedgerSequence > LastLedgerSequence\n";
        return EMISSION_FAILURE;
    }


    // rule 7 check the emitted txn pays the appropriate fee

    if (hookCtx.fee_base == 0)
        hookCtx.fee_base = etxn_fee_base(wasm_ctx, read_len);

    int64_t minfee = hookCtx.fee_base * hook_api::drops_per_byte * read_len;
    if (minfee < 0 || hookCtx.fee_base < 0)
    {
        JLOG(j.trace()) << "Hook emission failure: fee could not be calculated\n";
        return EMISSION_FAILURE;
    }

    if (!stpTrans->isFieldPresent(sfFee))
    {
        JLOG(j.trace()) << "Hook emission failure: Fee missing from emitted tx\n";
        return EMISSION_FAILURE;
    }

    int64_t fee = stpTrans->getFieldAmount(sfFee).xrp().drops();
    if (fee < minfee)
    {
        JLOG(j.trace()) << "Hook emission failure: Fee on emitted txn is less than the minimum required fee\n";
        return EMISSION_FAILURE;
    }

    std::string reason;
    auto tpTrans = std::make_shared<Transaction>(stpTrans, reason, app);
    if (tpTrans->getStatus() != NEW)
    {
        JLOG(j.trace()) << "Hook emission failure: tpTrans->getStatus() != NEW\n";
        return EMISSION_FAILURE;
    }

    hookCtx.result.emittedTxn.push(tpTrans);

    return read_len;
}

// When implemented will return the hash of the current hook
int64_t hook_api::hook_hash (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t write_ptr, uint32_t ptr_len )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    return NOT_IMPLEMENTED; // RH TODO implement
}

// Write the account id that the running hook is installed on into write_ptr
int64_t hook_api::hook_account (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t write_ptr, uint32_t ptr_len )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    if (NOT_IN_BOUNDS(write_ptr, 20, memory_length))
        return OUT_OF_BOUNDS;

    WRITE_WASM_MEMORY_AND_RETURN(
        write_ptr, 20,
        hookCtx.result.account.data(), 20,
        memory, memory_length);
}

// Deterministic nonces (can be called multiple times)
// Writes nonce into the write_ptr
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
            applyCtx.tx.getTransactionID(),
            hookCtx.nonce_counter++,
            hookCtx.result.account
    );

    hookCtx.nonce_used[hash] = true;

    WRITE_WASM_MEMORY_AND_RETURN(
        write_ptr, 32,
        hash.data(), 32,
        memory, memory_length);

    return 32;
}

// Reserve one or more transactions for emission from the running hook
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

// Compute the burden of an emitted transaction based on a number of factors
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

// When implemented this function will allow all other hook api functions to be called by number instead of
// name... this is important for size in some cases
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
    return NOT_IMPLEMENTED; // RH TODO, and bill appropriately
}


// RH NOTE this is a light-weight stobject parsing function for drilling into a provided serialzied object
// however it could probably be replaced by an existing class or routine or set of routines in XRPLD
// Returns object length including header bytes (and footer bytes in the event of array or object)
// negative indicates error
// -1 = unexpected end of bytes
// -2 = unknown type (detected early)
// -3 = unknown type (end of function)
// -4 = excessive stobject nesting
// -5 = excessively large array or object
inline int32_t get_stobject_length (
    unsigned char* start,   // in - begin iterator
    unsigned char* maxptr,  // in - end iterator
    int& type,              // out - populated by serialized type code
    int& field,             // out - populated by serialized field code
    int& payload_start,     // out - the start of actual payload data for this type
    int& payload_length,    // out - the length of actual payload data for this type
    int recursion_depth = 0)   // used internally
{
    if (recursion_depth > 10)
        return -4;

    unsigned char* end = maxptr;
    unsigned char* upto = start;
    int high = *upto >> 4;
    int low = *upto & 0xF;

    upto++; if (upto >= end) return -1;
    if (high > 0 && low > 0)
    {
        // common type common field
        type = high;
        field = low;
    } else if (high > 0) {
        // common type, uncommon field
        type = high;
        field = *upto++;
    } else if (low > 0) {
        // common field, uncommon type
        field = low;
        type = *upto++;
    } else {
        // uncommon type and field
        type = *upto++;
        if (upto >= end) return -1;
        field = *upto++;
    }

    DBG_PRINTF("%d get_st_object found field %d type %d\n", recursion_depth, field, type); 

    if (upto >= end) return -1;

    // RH TODO: link this to rippled's internal STObject constants
    // E.g.:
    /*
    int field_code = (safe_cast<int>(type) << 16) | field;
    auto const& fieldObj = ripple::SField::getField;
    */

    if (type < 1 || type > 19 || ( type >= 9 && type <= 13))
        return -2;

    bool is_vl = (type == 8 /*ACCID*/ || type == 7 || type == 18 || type == 19);


    int length = -1;
    if (is_vl)
    {
        length = *upto++;
        if (length == 0 || upto >= end)
            return -1;

        if (length < 193)
        {
            // do nothing
        } else if (length > 192 && length < 241)
        {
            length -= 193;
            length *= 256;
            length += *upto++ + 193; if (upto > end) return -1;
        } else {
            int b2 = *upto++; if (upto >= end) return -1;
            length -= 241;
            length *= 65536;
            length += 12481 + (b2 * 256) + *upto++; if (upto >= end) return -1;
        }
    } else if ((type >= 1 && type <= 5) || type == 16 || type == 17 )
    {
        length =    (type ==  1 ?  2 :
                    (type ==  2 ?  4 :
                    (type ==  3 ?  8 :
                    (type ==  4 ? 16 :
                    (type ==  5 ? 32 :
                    (type == 16 ?  1 :
                    (type == 17 ? 20 : -1 )))))));

    } else if (type == 6) /* AMOUNT */
    {
        length =  (*upto >> 6 == 1) ? 8 : 48;
        if (upto >= end) return -1;
    }

    if (length > -1)
    {
        payload_start = upto - start;
        payload_length = length;
        DBG_PRINTF("%d get_stobject_length field: %d Type: %d VL: %s Len: %d Payload_Start: %d Payload_Len: %d\n",
            recursion_depth, field, type, (is_vl ? "yes": "no"), length, payload_start, payload_length);
        return length + (upto - start);
    }

    if (type == 15 || type == 14) /* Object / Array */
    {
       payload_start = upto - start;

       for(int i = 0; i < 1024; ++i)
       {
            int subfield = -1, subtype = -1, payload_start_ = -1, payload_length_ = -1;
            int32_t sublength = get_stobject_length(
                    upto, end, subtype, subfield, payload_start_, payload_length_, recursion_depth + 1);
            DBG_PRINTF("%d get_stobject_length i %d %d-%d, upto %d sublength %d\n", recursion_depth, i,
                    subtype, subfield, upto - start, sublength);
            if (sublength < 0)
                return -1;
            upto += sublength;
            if (upto >= end)
                return -1;

            if ((*upto == 0xE1U && type == 0xEU) ||
                (*upto == 0xF1U && type == 0xFU))
            {
                payload_length = upto - start - payload_start;
                upto++;
                return (upto - start);
            }
       }
       return -5;
    }

    return -3;

}

// Given an serialized object in memory locate and return the offset and length of the payload of a subfield of that
// object. Arrays are returned fully formed. If successful returns offset and length joined as int64_t.
// Use SUB_OFFSET and SUB_LENGTH to extract.
int64_t hook_api::util_subfield (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t read_ptr, uint32_t read_len, uint32_t field_id )
{

    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    if (NOT_IN_BOUNDS(read_ptr, read_len, memory_length))
        return OUT_OF_BOUNDS;

    if (read_len < 1)
        return TOO_SMALL;

    unsigned char* start = (unsigned char*)(memory + read_ptr);
    unsigned char* upto = start;
    unsigned char* end = start + read_len;

    DBG_PRINTF("util_subfield called, looking for field %u type %u\n", field_id & 0xFFFF, (field_id >> 16));
    for (int j = -5; j < 5; ++j)
        DBG_PRINTF(( j == 0 ? " >%02X< " : "  %02X  "), *(start + j));
    DBG_PRINTF("\n");

    if (*upto & 0xF0 == 0xE0)
        upto++;

    for (int i = 0; i < 1024 && upto < end; ++i)
    {
        int type = -1, field = -1, payload_start = -1, payload_length = -1;
        int32_t length = get_stobject_length(upto, end, type, field, payload_start, payload_length, 0);
        if (length < 0)
            return PARSE_ERROR;
        if ((type << 16) + field == field_id)
        {
            DBG_PRINTF("util_subfield returned for field %u type %u\n", field_id & 0xFFFF, (field_id >> 16));
            for (int j = -5; j < 5; ++j)
                DBG_PRINTF(( j == 0 ? " [%02X] " : "  %02X  "), *(upto + j));
            DBG_PRINTF("\n");
            if (type == 0xF)    // we return arrays fully formed
                return (((int64_t)(upto - start)) << 32) /* start of the object */
                    + (uint32_t)(length);
            // return pointers to all other objects as payloads
            return (((int64_t)(upto - start + payload_start)) << 32) /* start of the object */
                + (uint32_t)(payload_length);
        }
        upto += length;
    }

    return DOESNT_EXIST;
}

// Same as subfield but indexes into a serialized array
int64_t hook_api::util_subarray (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t read_ptr, uint32_t read_len, uint32_t index_id )
{

    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    if (NOT_IN_BOUNDS(read_ptr, read_len, memory_length))
        return OUT_OF_BOUNDS;

    if (read_len < 1)
        return TOO_SMALL;

    unsigned char* start = (unsigned char*)(memory + read_ptr);
    unsigned char* upto = start;
    unsigned char* end = start + read_len;

    if (*upto & 0xF0 == 0xF0)
        upto++;

    DBG_PRINTF("util_subarray called, looking for index %u\n", index_id);
    for (int j = -5; j < 5; ++j)
        printf(( j == 0 ? " >%02X< " : "  %02X  "), *(start + j));
    DBG_PRINTF("\n");

    for (int i = 0; i < 1024 && upto < end; ++i)
    {
        int type = -1, field = -1, payload_start = -1, payload_length = -1;
        int32_t length = get_stobject_length(upto, end, type, field, payload_start, payload_length, 0);
        if (length < 0)
            return PARSE_ERROR;
        if (i == index_id)
        {
            DBG_PRINTF("util_subarray returned for index %u\n", index_id);
            for (int j = -5; j < 5; ++j)
                DBG_PRINTF(( j == 0 ? " [%02X] " : "  %02X  "), *(upto + j + length));
            DBG_PRINTF("\n");

            return (((int64_t)(upto - start)) << 32) /* start of the object */
                +   (uint32_t)(length);
        }
        upto += length;
    }

    return DOESNT_EXIST;
}

// Convert an account ID into a base58-check encoded r-address
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

// Convert a base58-check encoded r-address into a 20 byte account id
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

    if (read_len > 49)
        return TOO_BIG;

    // RH TODO we shouldn't need to slice this input but the base58 routine fails if we dont... maybe
    // some encoding or padding that shouldnt be there or maybe something that should be there
    char buffer[50];
    for (int i = 0; i < read_len; ++i)
        buffer[i] = *(memory + read_ptr + i);
    buffer[read_len] = 0;

    std::string raddr{buffer};
    std::cout << "util_accid raddr: " << raddr << "\n";

    auto const result = decodeBase58Token(raddr, TokenType::AccountID);
    if (result.empty())
        return INVALID_ARGUMENT;


    WRITE_WASM_MEMORY_AND_RETURN(
        write_ptr, write_len,
        result.data(), 20,
        memory, memory_length);
}

// when implemented this function will validate an st-object
int64_t hook_api::util_verify_sto (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t tread_ptr, uint32_t tread_len)
{

    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    return NOT_IMPLEMENTED;
/*
    ripple::Serializer ss { (void*)(memory + tread_ptr), tread_len };
    auto const sig = get(st, sigField);
    if (!sig)
        return false;
    Serializer ss;
    ss.add32(prefix);
    st.addWithoutSigningFields(ss);
    return verify(
        pk, Slice(ss.data(), ss.size()), Slice(sig->data(), sig->size()));
*/
}



// Validate either an secp256k1 signature or an ed25519 signature, using the XRPLD convention for identifying
// the key type. Pointer prefixes: d = data, s = signature, k = public key.
int64_t hook_api::util_verify (
        wasmer_instance_context_t * wasm_ctx,
        uint32_t dread_ptr, uint32_t dread_len,
        uint32_t sread_ptr, uint32_t sread_len,
        uint32_t kread_ptr, uint32_t kread_len )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack

    if (NOT_IN_BOUNDS(dread_ptr, dread_len, memory_length) ||
        NOT_IN_BOUNDS(sread_ptr, sread_len, memory_length) ||
        NOT_IN_BOUNDS(kread_ptr, kread_len, memory_length))
        return OUT_OF_BOUNDS;

    ripple::Slice keyslice  {reinterpret_cast<const void*>(kread_ptr + memory), kread_len};
    ripple::Slice data {reinterpret_cast<const void*>(dread_ptr + memory), dread_len};
    ripple::Slice sig  {reinterpret_cast<const void*>(sread_ptr + memory), sread_len};
    ripple::PublicKey key { keyslice };
    return verify(key, data, sig, false) ? 1 : 0;
}

// Return the current fee base of the current ledger (multiplied by a margin)
int64_t hook_api::fee_base (
        wasmer_instance_context_t * wasm_ctx )
{
    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, applyCtx, hookCtx on current stack
    return (int64_t)((double)(view.fees().base.drops()) * hook_api::fee_base_multiplier);
}

// Return the fee base for a hypothetically emitted transaction from the current hook based on byte count
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

// Populate an sfEmitDetails field in a soon-to-be emitted transaction
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
    DBG_PRINTF("emitdetails size = %d\n", (out - memory - write_ptr));
    return 105;
}


// RH TODO: bill based on guard counts
// Guard function... very important. Enforced on SetHook transaction, keeps track of how many times a
// runtime loop iterates and terminates the hook if the iteration count rises above a preset number of iterations
// as determined by the hook developer
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
        {
            JLOG(j.trace()) << "Hook macro guard violation. Src line: " << (id & 0xFFFFU) <<
                              " Macro line: " << (id >> 16) << " Iterations: " << hookCtx.guard_map[id] << "\n";
        }
        else
        {
            JLOG(j.trace()) << "Hook guard violation. Src line: " << id <<
                " Iterations: " << hookCtx.guard_map[id] << "\n";
        }
        rollback(wasm_ctx, 0, 0, GUARD_VIOLATION);
    }
    return 1;
}

