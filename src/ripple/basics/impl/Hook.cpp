#include <ripple/basics/Hook.h>
#include <ripple/app/tx/impl/ApplyContext.h>


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
