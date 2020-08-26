#ifndef RIPPLE_BASICS_HOOK_H_INCLUDED
#define RIPPLE_BASICS_HOOK_H_INCLUDED
#include "wasmer.hh"

#define WI32 (wasmer_value_tag::WASM_I32)
#define WI64 (wasmer_value_tag::WASM_I64)

#endif
namespace hook_internal {
    template <typename F>
    wasmer_import_t func_import ( F func, std::string_view call_name, std::initializer_list<wasmer_value_tag> func_params );
}

namespace hook_api {
    int64_t output_dbg ( wasmer_instance_context_t * wasm_ctx, uint32_t ptr, uint32_t len );
    int64_t get_current_ledger_id ( wasmer_instance_context_t * wasm_ctx, uint32_t ptr );
}

// templates must be defined in the same file they are declared in, otherwise this would go in impl/Hook.cpp
#ifndef RIPPLE_BASICS_HOOK_H_TEMPLATES
#define RIPPLE_BASICS_HOOK_H_TEMPLATES
template <typename F>
wasmer_import_t hook_internal::func_import ( F func, std::string_view call_name, std::initializer_list<wasmer_value_tag> func_params ) {
    return
    {   .module_name = { .bytes = (const uint8_t *) "env", .bytes_len = 3 },
        .import_name = { .bytes = (const uint8_t *) call_name.data(), .bytes_len = call_name.size() },
        .tag = wasmer_import_export_kind::WASM_FUNCTION,
        .value = { .func =
            wasmer_import_func_new(
                reinterpret_cast<void (*)(void*)>(func),
                std::begin( func_params ),
                func_params.size(),
                std::begin( { WI64 } ),
                1
            )
        }
    };
}
#endif
