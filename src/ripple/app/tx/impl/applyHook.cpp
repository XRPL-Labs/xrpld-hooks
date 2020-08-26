#include <ripple/app/tx/applyHook.h>
using namespace ripple;
/*
static TER
hook::removeHookStateFromLedger(
    Application& app,
    ApplyView& view,
    Keylet const& accountKeylet,
    Keylet const& ownerDirKeylet,
    Keylet const& hookStateKeylet)
{
    SLE::pointer hook = view.peek(hookKeylet);

    // If the signer list doesn't exist we've already succeeded in deleting it.
    if (!hookState)
        return tesSUCCESS;

    // Remove the node from the account directory.
    auto const hint = (*hookState)[sfOwnerNode];

    if (!view.dirRemove(ownerDirKeylet, hint, hookStateKeylet.key, false))
    {
        return tefBAD_LEDGER;
    }

    adjustOwnerCount(
        view,
        view.peek(accountKeylet),
        -1,
        app.journal("View"));

    // remove the actual hook
    view.erase(hookState);

    return tesSUCCESS;
}

TER
hook::replaceHook()
{
    auto const accountKeylet = keylet::account(account_);
    auto const ownerDirKeylet = keylet::ownerDir(account_);
    auto const hookKeylet = keylet::hook(account_);

    // This may be either a create or a replace.  Preemptively remove any
    // old hook.  May reduce the reserve, so this is done before
    // checking the reserve.
    if (TER const ter = removeHookFromLedger(
            ctx_.app, view(), accountKeylet, ownerDirKeylet, hookKeylet))
        return ter;

    auto const sle = view().peek(accountKeylet);
    if (!sle)
        return tefINTERNAL;

    // Compute new reserve.  Verify the account has funds to meet the reserve.
    std::uint32_t const oldOwnerCount{(*sle)[sfOwnerCount]};


    int addedOwnerCount{1};

    XRPAmount const newReserve{
        view().fees().accountReserve(oldOwnerCount + addedOwnerCount)};

    // We check the reserve against the starting balance because we want to
    // allow dipping into the reserve to pay fees.  This behavior is consistent
    // with CreateTicket.
    if (mPriorBalance < newReserve)
        return tecINSUFFICIENT_RESERVE;

    // Everything's ducky.  Add the ltHOOK to the ledger.
    auto hook = std::make_shared<SLE>(hookKeylet);
    view().insert(hook);
    writeHookToSLE(hook);

    auto viewJ = ctx_.app.journal("View");
    // Add the hook to the account's directory.
    auto const page = dirAdd(
        ctx_.view(),
        ownerDirKeylet,
        hookKeylet.key,
        false,
        describeOwnerDir(account_),
        viewJ);

    JLOG(j_.trace()) << "Create hook for account " << toBase58(account_)
                     << ": " << (page ? "success" : "failure");

    if (!page)
        return tecDIR_FULL;

    // If we succeeded, the new entry counts against the
    // creator's reserve.
    adjustOwnerCount(view(), sle, addedOwnerCount, viewJ);
    return tesSUCCESS;
}

TER
Hook::destroyHookState()
{
    auto const accountKeylet = keylet::account(account_);
    SLE::pointer ledgerEntry = view().peek(accountKeylet);
    if (!ledgerEntry)
        return tefINTERNAL;

    auto const ownerDirKeylet = keylet::ownerDir(account_);
    auto const hookKeylet = keylet::hook(account_);
    return removeHookFromLedger(
        ctx_.app, view(), accountKeylet, ownerDirKeylet, hookKeylet);
}

void
Hook::writeHookStateToSLE(
    SLE::pointer const& ledgerEntry) const
{
    //todo: [RH] support flags?
    ledgerEntry->setFieldVL(sfCreateCode, hook_);
}
*/


void hook::print_wasmer_error()
{
  int error_len = wasmer_last_error_length();
  char *error_str = (char*)malloc(error_len);
  wasmer_last_error_message(error_str, error_len);
  printf("Error: `%s`\n", error_str);
    free(error_str);
}

TER hook::apply(Blob hook, ApplyContext& apply_ctx) {

    wasmer_instance_t *instance = NULL;


    if (wasmer_instantiate(&instance, hook.data(), hook.size(), imports, 1) != wasmer_result_t::WASMER_OK) {
        printf("hook malformed\n");
        print_wasmer_error();
        return temMALFORMED;
    }

    wasmer_instance_context_data_set ( instance, &apply_ctx );
        printf("Set ApplyContext: %lx\n", (void*)&apply_ctx);

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

    ripple::ApplyContext* apply_ctx = (ripple::ApplyContext*) wasmer_instance_context_data_get(wasm_ctx);

    uint8_t *memory = wasmer_memory_data( wasmer_instance_context_memory(wasm_ctx, 0) );
    printf("HOOKAPI_output_dbg: ");
    if (len > 1024) len = 1024;
    for (int i = 0; i < len; ++i)
        printf("%c", memory[ptr + i]);
    return len;
}

int64_t hook_api::get_current_ledger_id ( wasmer_instance_context_t * wasm_ctx, uint32_t ptr ) {
    ripple::ApplyContext* apply_ctx = (ripple::ApplyContext*) wasmer_instance_context_data_get(wasm_ctx);
    uint8_t *memory = wasmer_memory_data( wasmer_instance_context_memory(wasm_ctx, 0) );
}
