#include <ripple/basics/Blob.h>
#include <ripple/protocol/TER.h>
#include <ripple/app/tx/impl/ApplyContext.h>
#include <ripple/beast/utility/Journal.h>    
#include "wasmer.hh"

namespace hook_api {

    enum api_return_code {
        SUCCESS = 0, // return codes > 0 are reserved for hook apis to return "success" with bytes read/written
        OUT_OF_BOUNDS = -1
    };

    // this is the api that wasm modules use to communicate with rippled
    int64_t output_dbg ( wasmer_instance_context_t * wasm_ctx, uint32_t ptr, uint32_t len );
    int64_t set_state ( wasmer_instance_context_t * wasm_ctx, uint32_t ptr key_ptr, uint32_t data_ptr_in, uint32_t in_len );
    int64_t get_state ( wasmer_instance_context_t * wasm_ctx, uint32_t ptr key_ptr, uint32_t data_ptr_out, uint32_t out_len );

    //   int64_t get_current_ledger_id ( wasmer_instance_context_t * wasm_ctx, uint32_t ptr );
}

namespace hook {

    constexpr state_max_blob_size = 128;

    void print_wasmer_error();
    ripple::TER apply(ripple::Blob hook, ripple::ApplyContext& apply_ctx);
    template <typename F>
    wasmer_import_t functionImport ( F func, std::string_view call_name, std::initializer_list<wasmer_value_tag> func_params );

#ifndef RIPPLE_HOOK_H_INCLUDED
#define RIPPLE_HOOK_H_INCLUDED
#define WI32 (wasmer_value_tag::WASM_I32)
#define WI64 (wasmer_value_tag::WASM_I64)
    wasmer_import_t imports[] = {
        functionImport ( hook_api::output_dbg,  "output_dbg",   { WI32, WI32        } ),
        functionImport ( hook_api::set_state,   "set_state",    { WI32, WI32, WI32  } ),
        functionImport ( hook_api::get_state,   "get_state",    { WI32, WI32, WI32  } )
   //     functionImport ( hook_api::get_current_ledger_id, "get_current_ledger_id", { WI32 } )
    };

#define HOOK_SETUP()\
    ApplyContext* apply_ctx = (ApplyContext*) wasmer_instance_context_data_get( wasm_ctx );\
    beast::Journal& j = apply_ctx.journal;\
    wasmer_memory_t* memory_ctx = wasmer_instance_context_memory( wasm_ctx, 0 );\
    uint8_t* memory = wasmer_memory_data( memory_ctx );\
    uint32_t memory_length = wasmer_memory_data_length ( memory_ctx );    


#else
    extern wasmer_import_t imports[];
#endif

}


#ifndef RIPPLE_HOOK_H_TEMPLATES
#define RIPPLE_HOOK_H_TEMPLATES
// templates must be defined in the same file they are declared in, otherwise this would go in impl/Hook.cpp
template <typename F>
wasmer_import_t hook::functionImport ( F func, std::string_view call_name, std::initializer_list<wasmer_value_tag> func_params ) {
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
