#include <ripple/app/tx/applyHook.h>
#include <ripple/basics/Log.h>
#include <ripple/basics/Slice.h>
using namespace ripple;

TER
hook::setHookState(
    HookContext& hookCtx,
    Keylet const& hookStateKeylet,
    Slice& data
){

    auto const sle = view().peek(accountKeylet);
    if (!sle)
        return tefINTERNAL;

    auto const hook = view().peek(hookKeylet);
    if (!hook) { // [RH] should this be more than trace??
        JLOG(j.trace()) << "Attempted to set a hook state for a hook that doesnt exist " << toBase58(account);
        return tefINTERNAL;
    }

    uint32_t hookDataMax = hook->getFieldU32(sfHookDataMaxSize);

    // if the blob is too large don't set it
    if (data.size() > hookDataMax) {
       return temHOOK_DATA_TOO_LARGE; 
    } 

    uint32_t stateCount = hook->getFieldU32(sfHookStateCount);
    uint32_t oldStateReserve = COMPUTE_HOOK_DATA_OWNER_COUNT(stateCount);

    auto const oldHookState = view.peek(oldHookState);

    // if the blob is nil then delete the entry if it exists
    if (data.size() == 0) {
    
        if (!view.peek(hookStateKeylet))
            return tesSUCCESS; // a request to remove a non-existent entry is defined as success

        auto const hint = (*hookState)[sfOwnerNode];

        // Remove the node from the account directory.
        if (!view.dirRemove(ownerDirKeylet, hint, hookStateKeylet.key, false))
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
            adjustOwnerCount(view(), sle, -1, app.journal("View"));
        
        hook->setFieldU32(sfHookStateCount, COMPUTE_HOOK_DATA_OWNER_COUNT(stateCount));

        return tesSUCCESS;
    }

    
    std::uint32_t const ownerCount{(*sle)[sfOwnerCount]};

    // execution to this point means we are updating or creating a state hook
    auto const hookStateOld = view().peek(hookStateKeylet);
    if (hookStateOld) { 
        view.erase(hookStateOld);
    } else {

        ++stateCount;

        if (COMPUTE_HOOK_DATA_OWNER_COUNT(stateCount) > oldStateReserve) {
            // the hook used its allocated allotment of state entries for its previous ownercount
            // increment ownercount and give it another allotment
    
            ++ownerCount;
            XRPAmount const newReserve{
                view().fees().accountReserve(ownerCount)};

            if (STAmount((*sle)[sfBalance]).xrp() < newReserve)
                return tecINSUFFICIENT_RESERVE;

            
            adjustOwnerCount(view(), sle, 1, app.journal("View"));

        }

        // update state count
        hook->setFieldU32(sfHookStateCount, stateCount);

    }

    // add new data to ledger
    auto hookStateNew = std::make_shared<SLE>(hookStateKeylet);
    view().insert(hookStateNew);
    hookStateNew->setFieldVL(sfHookData, data);

    if (!hookStateOld) {
        auto viewJ = j("View");
        // Add the hook to the account's directory if it wasn't there already
        auto const page = dirAdd(
            view,
            ownerDirKeylet,
            hookStateKeylet.key,
            false,
            describeOwnerDir(account),
            viewJ);
        
        JLOG(j.trace()) << "Create/update hook state for account " << toBase58(account)
                     << ": " << (page ? "success" : "failure");
        
        if (!page)
            return tecDIR_FULL;

        hookStateNew->setFieldU64(sfOwnerNode, *page);

    }

    return tesSUCCESS;
}

void hook::print_wasmer_error()
{
  int error_len = wasmer_last_error_length();
  char *error_str = (char*)malloc(error_len);
  wasmer_last_error_message(error_str, error_len);
  printf("Error: `%s`\n", error_str);
    free(error_str);
}

TER hook::apply(Blob hook, ApplyContext& apply_ctx, AccountID& account) {

    wasmer_instance_t *instance = NULL;


    if (wasmer_instantiate(&instance, hook.data(), hook.size(), imports, 1) != wasmer_result_t::WASMER_OK) {
        printf("hook malformed\n");
        print_wasmer_error();
        return temMALFORMED;
    }


    HookContext hook_ctx {
        .apply_ctx = apply_ctx,
        .account = account,
        .accountKeylet = keylet::account(account),
        .ownerDirKeylet = keylet::ownerDir(account),
        .hookKeylet = keylet::hook(account)
    };

    wasmer_instance_context_data_set ( instance, &hook_ctx );
    printf("Set HookContext: %lx\n", (void*)&hook_ctx);

    wasmer_value_t arguments[] = { { .tag = wasmer_value_tag::WASM_I64, .value = {.I64 = 0 } } };
    wasmer_value_t results[] = { { .tag = wasmer_value_tag::WASM_I64, .value = {.I64 = 0 } } };

    if (wasmer_instance_call(
        instance,
        "hook",
        arguments,
        1,
        results,
        1
    ) != wasmer_result_t::WASMER_OK) {
        printf("hook() call failed\n");
        print_wasmer_error();
        return temMALFORMED; /// todo: [RH] should be a hook execution error code tecHOOK_ERROR?
    }

    int64_t response_value = results[0].value.I64;

    printf("hook return code was: %ld\n", response_value);

    // todo: [RH] memory leak here, destroy the imports, instance using a smart pointer
    wasmer_instance_destroy(instance);
    printf("running hook 3\n");

    return tesSUCCESS;
}


int64_t hook_api::output_dbg ( wasmer_instance_context_t * wasm_ctx, uint32_t ptr, uint32_t len ) {

    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, apply_ctx on current stack

    printf("HOOKAPI_output_dbg: ");
    if (len > 1024) len = 1024;
    for (int i = 0; i < len && i < memory_length; ++i)
        printf("%c", memory[ptr + i]);
    return len;

}
int64_t hook_api::set_state ( wasmer_instance_context_t * wasm_ctx, uint32_t key_ptr, uint32_t data_ptr_in, uint32_t in_len ) {

    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, apply_ctx, hook_ctx on current stack

    if (key_ptr + 32 > memory_length || data_ptr_in + hook::state_max_blob_size > memory_length) {
        JLOG(j.trace())
            << "Hook tried to set_state using memory outside of the wasm instance limit";
        return OUT_OF_BOUNDS;
    }
    
    if (in_len == 0)
        return TOO_SMALL;

    auto const sle = apply_ctx.view().peek(hook_ctx.hookKeylet);
    if (!sle)
        return INERNAL_ERROR;
    
    auto HSKeylet = keylet::hook_state(hook_ctx.account, ripple::uint256::fromVoid(memory + key_ptr));

    uint32_t maxSize = sle->getFieldU32(sfHookDataMaxSize); 
    if (in_len > maxSize)
        return TOO_BIG;
   
    // execution to here means we can store state
    if ( TER const result = setHookState(hook_ctx, HSKeylet, Slice(memory + data_ptr_in,  in_len)) )
        return TER_TO_HOOK_RETURN_CODE(result);

    return in_len;
}

int64_t hook_api::get_state ( wasmer_instance_context_t * wasm_ctx, uint32_t key_ptr, uint32_t data_ptr_out ) {

    HOOK_SETUP(); // populates memory_ctx, memory, memory_length, apply_ctx, hook_ctx on current stack

    if (key_ptr + 32 > memory_length) {
        JLOG(j.trace())
            << "Hook tried to get_state using memory outside of the wasm instance limit";
        return OUT_OF_BOUNDS;
    }
    
    auto const sle = apply_ctx.view().peek(hook_ctx.hookKeylet);
    if (!sle)
        return INERNAL_ERROR;

    auto HSKeylet = keylet::hook_state(hook_ctx.account, ripple::uint256::fromVoid(memory + key_ptr));
    if (!HSKeylet)
        return DOESNT_EXIST;
    
    Blob b = HSKeylet->getFieldVL(sfHookData);

    if (data_ptr_out + b.size() > memory_length) {
        JLOG(j.trace())
            << "Hook: get_state tried to retreive blob of " << b.size() << " bytes past end of wasm memory";
        return OUT_OF_BOUNDS;
    }

    ::memcpy(data_ptr_out, b.data(), b.size());

    return b.size();

}

/*int64_t hook_api::get_current_ledger_id ( wasmer_instance_context_t * wasm_ctx, uint32_t ptr ) {
    ripple::ApplyContext* apply_ctx = (ripple::ApplyContext*) wasmer_instance_context_data_get(wasm_ctx);
    uint8_t *memory = wasmer_memory_data( wasmer_instance_context_memory(wasm_ctx, 0) );
}*/
