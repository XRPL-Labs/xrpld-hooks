#include <ripple/basics/Blob.h>
#include <ripple/protocol/TER.h>
#include <ripple/app/tx/impl/ApplyContext.h>
#include <ripple/beast/utility/Journal.h>
//#include <ripple/nodestore/NodeObject.h>
#include <ripple/app/misc/Transaction.h>
#include <queue>
#include "wasmer.hh"


namespace hook_api {

#define TER_TO_HOOK_RETURN_CODE(x)\
    (((TERtoInt(x)) << 16)*-1)
    
#ifndef RIPPLE_HOOK_H_INCLUDED1
#define RIPPLE_HOOK_H_INCLUDED1
    enum api_return_code {
        SUCCESS = 0,                    // return codes > 0 are reserved for hook apis to return "success" with bytes read/written
        OUT_OF_BOUNDS = -1,             // could not read or write to a pointer to provided by hook because it would be out of bounds
        INTERNAL_ERROR = -2,            // eg directory is corrupt
        TOO_BIG = -3,                   // something you tried to store was too big
        TOO_SMALL = -4,                 // something you tried to store or provide was too small
        DOESNT_EXIST = -5,              // something you requested wasn't found
        NO_FREE_SLOTS = -6,             // when trying to load an object there is a maximum of 255 slots
        INVALID_ARGUMENT = -7,          // self explanatory
        ALREADY_SET = -8,               // returned when a one-time parameter was already set by the hook
        PREREQUISITE_NOT_MET = -9,      // returned if a required param wasn't set, before calling
        FEE_TOO_LARGE = -10             // returned if the attempted operation would result in an absurd fee
    };
    // less than 0xFFFF  : remove sign bit and shift right 16 bits and this is a TER code

    enum ExitType : int8_t {
        ROLLBACK = 0,
        ACCEPT = 1,
        REJECT = 2,
    };

    
#endif

    // this is the api that wasm modules use to communicate with rippled
    int64_t _exit                   ( wasmer_instance_context_t * wasm_ctx, int32_t error_code, uint32_t data_ptr_in, uint32_t in_len, ExitType exitType );
    int64_t accept                  ( wasmer_instance_context_t * wasm_ctx, int32_t error_code, uint32_t data_ptr_in, uint32_t in_len );
    int64_t emit_txn                ( wasmer_instance_context_t * wasm_ctx, uint32_t tx_ptr, uint32_t len );
    int64_t get_burden              ( wasmer_instance_context_t * wasm_ctx );
    int64_t get_emit_burden         ( wasmer_instance_context_t * wasm_ctx);
    int64_t get_emit_fee_base       ( wasmer_instance_context_t * wasm_ctx, uint32_t tx_byte_count);
    int64_t get_fee_base            ( wasmer_instance_context_t * wasm_ctx );
    int64_t get_generation          ( wasmer_instance_context_t * wasm_ctx );
    int64_t get_hook_account        ( wasmer_instance_context_t * wasm_ctx, uint32_t out_ptr, uint32_t out_len );
    int64_t get_ledger_seq          ( wasmer_instance_context_t * wasm_ctx );
    int64_t get_nonce               ( wasmer_instance_context_t * wasm_ctx, uint32_t out_ptr );
    int64_t get_obj_by_hash         ( wasmer_instance_context_t * wasm_ctx, uint32_t hash_ptr );
    int64_t get_pseudo_details      ( wasmer_instance_context_t * wasm_ctx, uint32_t ptr_out, uint32_t out_len );
    int64_t get_pseudo_details_size ( wasmer_instance_context_t * wasm_ctx );
    int64_t get_state               ( wasmer_instance_context_t * wasm_ctx, uint32_t key_ptr, uint32_t data_ptr_out, uint32_t out_len );
    int64_t get_txn_field           ( wasmer_instance_context_t * wasm_ctx, uint32_t field_id, uint32_t data_ptr_out, uint32_t out_len );
    int64_t get_txn_id              ( wasmer_instance_context_t * wasm_ctx );
    int64_t get_txn_type            ( wasmer_instance_context_t * wasm_ctx );
    int64_t output_dbg              ( wasmer_instance_context_t * wasm_ctx, uint32_t ptr, uint32_t len );
    int64_t output_dbg_obj          ( wasmer_instance_context_t * wasm_ctx, uint32_t slot );
    int64_t reject                  ( wasmer_instance_context_t * wasm_ctx, int32_t error_code, uint32_t data_ptr_in, uint32_t in_len );
    int64_t rollback                ( wasmer_instance_context_t * wasm_ctx, int32_t error_code, uint32_t data_ptr_in, uint32_t in_len );
    int64_t set_emit_count          ( wasmer_instance_context_t * wasm_ctx, uint32_t c );
    int64_t set_state               ( wasmer_instance_context_t * wasm_ctx, uint32_t key_ptr, uint32_t data_ptr_in, uint32_t in_len );




    //   int64_t get_current_ledger_id ( wasmer_instance_context_t * wasm_ctx, uint32_t ptr );
}

namespace hook {

    bool canHook(ripple::TxType txType, uint64_t hookOn);

    void printWasmerError();

    ripple::TER apply(ripple::Blob, ripple::ApplyContext&, const ripple::AccountID&);

    struct HookContext;

    int maxHookDataSize(void);

#ifndef RIPPLE_HOOK_H_INCLUDED
#define RIPPLE_HOOK_H_INCLUDED
    struct HookContext {
        ripple::ApplyContext& applyCtx;
        const ripple::AccountID& account;
        ripple::Keylet const& accountKeylet;
        ripple::Keylet const& ownerDirKeylet;
        ripple::Keylet const& hookKeylet;
        // uint256 key -> [ has_been_modified, current_state ]
        std::shared_ptr<std::map<ripple::uint256 const, std::pair<bool, ripple::Blob>>> changedState;
        hook_api::ExitType exitType;
        std::string exitReason {""};
        int64_t exitCode {-1};
        std::map<int, std::shared_ptr<ripple::Transaction>> slot;
        int slot_counter { 1 };
        std::queue<int> slot_free {};
        int64_t expected_emit_count { -1 }; // make this a 64bit int so the uint32 from the hookapi cant overflow it
        int nonce_counter { 0 }; // incremented whenever get_nonce is called to ensure unique nonces
    };

    //todo: [RH] change this to a validator votable figure
    int maxHookDataSize(void) {
        return 128;
    }

    ripple::TER
    setHookState(
        HookContext& hookCtx,
        ripple::Keylet const& hookStateKeylet,
        ripple::Slice& data
    );

    // finalize the changes the hook made to the ledger
    void commitChangesToLedger( HookContext& hookCtx );

    template <typename F>
    wasmer_import_t functionImport ( F func, std::string_view call_name, std::initializer_list<wasmer_value_tag> func_params );


#define COMPUTE_HOOK_DATA_OWNER_COUNT(state_count)\
    (std::ceil( (double)state_count/(double)5.0 )) 
#define WI32 (wasmer_value_tag::WASM_I32)
#define WI64 (wasmer_value_tag::WASM_I64)
    const int imports_count = 23;
    wasmer_import_t imports[] = {
        functionImport ( hook_api::accept,                      "accept",                   { WI32, WI32, WI32  } ),
        functionImport ( hook_api::emit_txn,                    "emit_txn"                  { WI32, WI32        } ),
        functionImport ( hook_api::get_burden,                  "get_burden",               {                   } ),
        functionImport ( hook_api::get_emit_burden,             "get_emit_burden",          {                   } ),
        functionImport ( hook_api::get_emit_fee_base,           "get_emit_fee_base",        { WI32              } ),

        functionImport ( hook_api::get_fee_base,                "get_fee_base",             {                   } ),
        functionImport ( hook_api::get_generation,              "get_generation",           {                   } ),
        functionImport ( hook_api::get_hook_account,            "get_hook_account",         { WI32, WI32        } ),
        functionImport ( hook_api::get_ledger_seq,              "get_ledger_seq",           {                   } ),
        functionImport ( hook_api::get_nonce,                   "get_nonce",                { WI32              } ),

        functionImport ( hook_api::get_obj_by_hash,             "get_obj_by_hash",          { WI32              } ),
        functionImport ( hook_api::get_pseudo_details,          "get_pseudo_details",       { WI32, WI32        } ),
        functionImport ( hook_api::get_pseudo_details_size,     "get_pseudo_details_size",  {                   } ),
        functionImport ( hook_api::get_state,                   "get_state",                { WI32, WI32, WI32  } ),
        functionImport ( hook_api::get_txn_field,               "get_txn_field",            { WI32, WI32, WI32  } ),
        
        functionImport ( hook_api::get_txn_id,                  "get_txn_id",               { WI32              } ),
        functionImport ( hook_api::get_txn_type,                "get_txn_type",             {                   } ),
        functionImport ( hook_api::output_dbg,                  "output_dbg",               { WI32, WI32        } ),
        functionImport ( hook_api::output_dbg_obj,              "output_dbg_obj",           { WI32              } ),
        functionImport ( hook_api::reject,                      "reject",                   { WI32, WI32, WI32  } ),
        
        functionImport ( hook_api::rollback,                    "rollback",                 { WI32, WI32, WI32  } ),
        functionImport ( hook_api::set_emit_count,              "set_emit_count",           { WI32              } ),
        functionImport ( hook_api::set_state,                   "set_state",                { WI32, WI32, WI32  } )
    };

    constexpr pseudo_details_size = 105;

#define HOOK_SETUP()\
    hook::HookContext& hookCtx = *((hook::HookContext*) wasmer_instance_context_data_get( wasm_ctx ));\
    ApplyContext& applyCtx = hookCtx.applyCtx;\
    auto& view = applyCtx.view();\
    auto j = applyCtx.app.journal("View");\
    const wasmer_memory_t* memory_ctx = wasmer_instance_context_memory( wasm_ctx, 0 );\
    uint8_t* memory = wasmer_memory_data( memory_ctx );\
    const uint64_t memory_length = wasmer_memory_data_length ( memory_ctx );    


#define WRITE_WASM_MEMORY_AND_RETURN(guest_dst_ptr, guest_dst_len, host_src_ptr, host_src_len, host_memory_ptr, guest_memory_length)\
    {int bytes_to_write = std::min(static_cast<int>(host_src_len), static_cast<int>(guest_dst_len));\
    if (guest_dst_ptr + bytes_to_write > guest_memory_length) {\
        JLOG(j.trace())\
            << "Hook: " << __func__ << " tried to retreive blob of " << host_src_len << " bytes past end of wasm memory";\
        return OUT_OF_BOUNDS;\
    }\
    ::memcpy(host_memory_ptr + guest_dst_ptr, host_src_ptr, bytes_to_write);\
    return bytes_to_write;}

// ptr = pointer inside the wasm memory space
#define NOT_IN_BOUNDS(ptr, len, memory_length)\
    (ptr > memory_length || static_cast<uint64_t>(ptr) + static_cast<uint64_t>(len) > static_cast<uint64_t>(memory_length))


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
