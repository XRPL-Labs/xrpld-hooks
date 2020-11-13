#include <ripple/basics/Blob.h>
#include <ripple/protocol/TER.h>
#include <ripple/app/tx/impl/ApplyContext.h>
#include <ripple/beast/utility/Journal.h>
//#include <ripple/nodestore/NodeObject.h>
#include <ripple/app/misc/Transaction.h>
#include <queue>
#include <optional>
#include "wasmer.hh"
#include <any>
#include <memory>

namespace hook_api {

#define TER_TO_HOOK_RETURN_CODE(x)\
    (((TERtoInt(x)) << 16)*-1)

#ifndef RIPPLE_HOOK_H_INCLUDED1
#define RIPPLE_HOOK_H_INCLUDED1

// for debugging if you want a lot of output change these to if (1)
#define DBG_PRINTF if (0) printf
#define DBG_FPRINTF if (0) fprintf

    enum api_return_code {
        SUCCESS = 0,                    // return codes > 0 are reserved for hook apis to return "success"
        OUT_OF_BOUNDS = -1,             // could not read or write to a pointer to provided by hook
        INTERNAL_ERROR = -2,            // eg directory is corrupt
        TOO_BIG = -3,                   // something you tried to store was too big
        TOO_SMALL = -4,                 // something you tried to store or provide was too small
        DOESNT_EXIST = -5,              // something you requested wasn't found
        NO_FREE_SLOTS = -6,             // when trying to load an object there is a maximum of 255 slots
        INVALID_ARGUMENT = -7,          // self explanatory
        ALREADY_SET = -8,               // returned when a one-time parameter was already set by the hook
        PREREQUISITE_NOT_MET = -9,      // returned if a required param wasn't set, before calling
        FEE_TOO_LARGE = -10,            // returned if the attempted operation would result in an absurd fee
        EMISSION_FAILURE = -11,         // returned if an emitted tx was not accepted by rippled
        TOO_MANY_NONCES = -12,          // a hook has a maximum of 256 nonces
        TOO_MANY_EMITTED_TXN = -13,     // a hook has emitted more than its stated number of emitted txn
        NOT_IMPLEMENTED = -14,          // an api was called that is reserved for a future version
        INVALID_ACCOUNT = -15,          // an api expected an account id but got something else
        GUARD_VIOLATION = -16,          // a guarded loop or function iterated over its maximum
        INVALID_FIELD = -17,            // the field requested is returning sfInvalid
        PARSE_ERROR = -18               // hook asked hookapi to parse something the contents of which was invalid
    };
    // less than 0xFFFF  : remove sign bit and shift right 16 bits and this is a TER code

    // many datatypes can be encoded into an int64_t
    int64_t data_as_int64(
            void* ptr_raw,
            uint32_t len)
    {
        unsigned char* ptr = reinterpret_cast<unsigned char*>(ptr_raw);
        if (len > 8)
            return TOO_BIG;
        uint64_t output = 0;
        for (int i = 0, j = (len-1)*8; i < len; ++i, j-=8)
            output += (((uint64_t)ptr[i]) << j);
        if ((1ULL<<63) & output)
            return TOO_BIG;
        return output;
    }

    enum ExitType : int8_t {
        UNSET = -2,
        WASM_ERROR = -1,
        ROLLBACK = 0,
        ACCEPT = 1,
    };

    const int etxn_details_size = 105;
    const int max_slots = 255;
    const int max_nonce = 255;
    const int max_emit = 255;
    const int drops_per_byte = 31250; //RH TODO make these  votable config option
    const double fee_base_multiplier = 1.1f;
#endif

    // RH NOTE: Find descriptions of api functions in ./impl/applyHook.cpp

    using wic_t = wasmer_instance_context_t;

    // the "special" _() api allows every other api to be invoked by a number (crc32 of name)
    // instead of function name
    int64_t _special            ( wic_t* w, uint32_t api_no,
                                            uint32_t a, uint32_t b, uint32_t c,
                                            uint32_t d, uint32_t e, uint32_t f );

    // not a real API, called by accept and rollback
    int64_t _exit               ( wic_t* w, uint32_t read_ptr, uint32_t read_len,
                                            int32_t error_code, ExitType exitType );

    // real apis start here ---
    int32_t _g                  ( wic_t* w, uint32_t guard_id, uint32_t maxiter );

    int64_t accept              ( wic_t* w, uint32_t read_ptr, uint32_t read_len, int32_t error_code );
    int64_t rollback            ( wic_t* w, uint32_t read_ptr, uint32_t read_len, int32_t error_code );
    int64_t util_raddr          ( wic_t* w, uint32_t write_ptr, uint32_t write_len,
                                            uint32_t read_ptr, uint32_t read_len );
    int64_t util_accid          ( wic_t* w, uint32_t write_ptr, uint32_t write_len,
                                            uint32_t read_ptr, uint32_t read_len );
    int64_t util_verify         ( wic_t* w, uint32_t dread_ptr, uint32_t dread_len,
                                            uint32_t sread_ptr, uint32_t sread_len,
                                            uint32_t kread_ptr, uint32_t kread_len );
    int64_t util_verify_sto     ( wic_t* w, uint32_t tread_ptr, uint32_t tread_len );
    int64_t util_sha512h        ( wic_t* w, uint32_t write_ptr, uint32_t write_len,
                                            uint32_t read_ptr,  uint32_t read_len );
    int64_t util_subfield       ( wic_t* w, uint32_t read_ptr, uint32_t read_len, uint32_t field_id );
    int64_t util_subarray       ( wic_t* w, uint32_t read_ptr, uint32_t read_len, uint32_t array_id );
    int64_t etxn_burden         ( wic_t* w );
    int64_t etxn_details        ( wic_t* w, uint32_t write_ptr, uint32_t write_len );
    int64_t etxn_fee_base       ( wic_t* w, uint32_t tx_byte_count);
    int64_t etxn_reserve        ( wic_t* w, uint32_t count );
    int64_t etxn_generation     ( wic_t* w );
    int64_t emit                ( wic_t* w, uint32_t read_ptr, uint32_t read_len );
    int64_t hook_account        ( wic_t* w, uint32_t write_ptr, uint32_t write_len );
    int64_t hook_hash           ( wic_t* w, uint32_t write_ptr, uint32_t write_len );
    int64_t fee_base            ( wic_t* w );
    int64_t ledger_seq          ( wic_t* w );
    int64_t nonce               ( wic_t* w, uint32_t write_ptr, uint32_t write_len );
    int64_t slot_clear          ( wic_t* w, uint32_t slot );
    int64_t slot_set            ( wic_t* w, uint32_t read_ptr, uint32_t read_len,
                                            uint32_t slot_type, int32_t slot );

    int64_t slot_field_txt      ( wic_t* w, uint32_t write_ptr, uint32_t write_len,
                                            uint32_t field_id, uint32_t slot );
    int64_t slot_field          ( wic_t* w, uint32_t write_ptr, uint32_t write_len,
                                            uint32_t field_id, uint32_t slot );
    int64_t slot_id             ( wic_t* w, uint32_t slot );
    int64_t slot_type           ( wic_t* w, uint32_t slot );
    int64_t state_set           ( wic_t* w, uint32_t read_ptr,  uint32_t read_len,
                                            uint32_t kread_ptr, uint32_t kread_len );
    int64_t state               ( wic_t* w, uint32_t write_ptr, uint32_t write_len,
                                            uint32_t kread_ptr, uint32_t kread_len );
    int64_t state_foreign       ( wic_t* w, uint32_t write_ptr, uint32_t write_len,
                                            uint32_t kread_ptr, uint32_t kread_len,
                                            uint32_t aread_ptr, uint32_t aread_len );
    int64_t trace_slot          ( wic_t* w, uint32_t slot );
    int64_t trace               ( wic_t* w, uint32_t read_ptr, uint32_t read_len, uint32_t as_hex );
    int64_t trace_num           ( wic_t* w, uint32_t read_ptr, uint32_t read_len, int64_t number );

    int64_t otxn_burden         ( wic_t* w );
    int64_t otxn_field          ( wic_t* w, uint32_t write_ptr, uint32_t write_len, uint32_t field_id );
    int64_t otxn_field_txt      ( wic_t* w, uint32_t write_ptr, uint32_t write_len, uint32_t field_id );
    int64_t otxn_generation     ( wic_t* w );
    int64_t otxn_id             ( wic_t* w, uint32_t write_ptr, uint32_t write_len );
    int64_t otxn_type           ( wic_t* w );


}

namespace hook {

    bool canHook(ripple::TxType txType, uint64_t hookOn);

    void printWasmerError(beast::Journal::Stream const& x);

    struct HookResult;

    HookResult apply(
            ripple::uint256,
            ripple::Blob,
            ripple::ApplyContext&,
            const ripple::AccountID&,
            bool callback);

    struct HookContext;

    int maxHookStateDataSize(void);

#ifndef RIPPLE_HOOK_H_INCLUDED
#define RIPPLE_HOOK_H_INCLUDED
    // account -> hook set tx id, already created wasmer instance
    std::map<ripple::AccountID, std::pair<ripple::uint256, wasmer_instance_t*>> hook_cache;
    std::mutex hook_cache_lock;

    struct HookResult
    {
        ripple::Keylet accountKeylet;
        ripple::Keylet ownerDirKeylet;
        ripple::Keylet hookKeylet;
        ripple::AccountID account;
        std::queue<std::shared_ptr<ripple::Transaction>> emittedTxn; // etx stored here until accept/rollback
        // uint256 key -> [ has_been_modified, current_state ]
        std::shared_ptr<std::map<ripple::uint256, std::pair<bool, ripple::Blob>>> changedState;
        hook_api::ExitType exitType = hook_api::ExitType::ROLLBACK;
        std::string exitReason {""};
        int64_t exitCode {-1};
    };

    struct HookContext {
        ripple::ApplyContext& applyCtx;
        // slots are used up by requesting objects from inside the hook
        // the map stores pairs consisting of a memory view and whatever shared or unique ptr is required to
        // keep the underlying object alive for the duration of the hook's execution
        std::map<int, std::pair<std::string_view, std::any>> slot;
        int slot_counter { 1 };
        std::queue<int> slot_free {};
        int64_t expected_etxn_count { -1 }; // make this a 64bit int so the uint32 from the hookapi cant overflow it
        int nonce_counter { 0 }; // incremented whenever nonce is called to ensure unique nonces
        std::map<ripple::uint256, bool> nonce_used;
        uint32_t generation = 0; // used for caching, only generated when txn_generation is called
        int64_t burden = 0;      // used for caching, only generated when txn_burden is called
        int64_t fee_base = 0;
        std::map<uint32_t, uint32_t> guard_map; // iteration guard map <id -> upto_iteration>
        HookResult result;
    };

    // RH TODO: fetch this value from the hook sle
    int maxHookStateDataSize(void) {
        return 128;
    }

    ripple::TER
    setHookState(
        HookResult& hookResult,
        ripple::ApplyContext& applyCtx,
        ripple::Keylet const& hookStateKeylet,
        ripple::uint256 key,
        ripple::Slice& data
    );

    // finalize the changes the hook made to the ledger
    void commitChangesToLedger( hook::HookResult& hookResult, ripple::ApplyContext& );

    template <typename F>
    wasmer_import_t functionImport ( F func, std::string_view call_name,
            std::initializer_list<wasmer_value_tag> func_params, int ret_type = 2);


    #define COMPUTE_HOOK_DATA_OWNER_COUNT(state_count)\
        (std::ceil( (double)state_count/(double)5.0 ))
    #define WI32 (wasmer_value_tag::WASM_I32)
    #define WI64 (wasmer_value_tag::WASM_I64)

    const int imports_count = 40;
    wasmer_import_t imports[] = {


        functionImport ( hook_api::_special,        "_",                { WI32, WI32, WI32, WI32,
                WI32, WI32                }),

        functionImport ( hook_api::_g,             "_g",               { WI32, WI32 }, 1 ),

        functionImport ( hook_api::accept,          "accept",           { WI32, WI32, WI32          }),
        functionImport ( hook_api::rollback,        "rollback",         { WI32, WI32, WI32          }),
        functionImport ( hook_api::util_raddr,      "util_raddr",       { WI32, WI32, WI32, WI32    }),
        functionImport ( hook_api::util_accid,      "util_accid",       { WI32, WI32, WI32, WI32    }),
        functionImport ( hook_api::util_verify,     "util_verify",      { WI32, WI32, WI32, WI32,
                                                                          WI32, WI32                }),
        functionImport ( hook_api::util_verify_sto, "util_verify_sto",  { WI32, WI32                }),
        functionImport ( hook_api::util_sha512h,    "util_sha512h",     { WI32, WI32, WI32, WI32    }),
        functionImport ( hook_api::util_subfield,   "util_subfield",    { WI32, WI32, WI32          }),
        functionImport ( hook_api::util_subarray,   "util_subarray",    { WI32, WI32, WI32          }),
        functionImport ( hook_api::emit,            "emit",             { WI32, WI32                }),
        functionImport ( hook_api::etxn_burden,     "etxn_burden",      {                           }),
        functionImport ( hook_api::etxn_fee_base,   "etxn_fee_base",    { WI32                      }),
        functionImport ( hook_api::etxn_details,    "etxn_details",     { WI32, WI32                }),
        functionImport ( hook_api::etxn_reserve,    "etxn_reserve",     { WI32                      }),
        functionImport ( hook_api::etxn_generation, "etxn_generation",  {                           }),
        functionImport ( hook_api::otxn_burden,     "otxn_burden",      {                           }),
        functionImport ( hook_api::otxn_generation, "otxn_generation",  {                           }),
        functionImport ( hook_api::otxn_field_txt,  "otxn_field_txt",   { WI32, WI32, WI32          }),
        functionImport ( hook_api::otxn_field,      "otxn_field",       { WI32, WI32, WI32          }),
        functionImport ( hook_api::otxn_id,         "otxn_id",          { WI32, WI32                }),
        functionImport ( hook_api::otxn_type,       "otxn_type",        {                           }),
        functionImport ( hook_api::hook_account,    "hook_account",     { WI32, WI32                }),
        functionImport ( hook_api::hook_hash,       "hook_hash",        { WI32, WI32                }),
        functionImport ( hook_api::fee_base,        "fee_base",         {                           }),
        functionImport ( hook_api::ledger_seq,      "ledger_seq",       {                           }),
        functionImport ( hook_api::nonce,           "nonce",            { WI32                      }),
        functionImport ( hook_api::state,           "state",            { WI32, WI32, WI32, WI32    }),
        functionImport ( hook_api::state_foreign,   "state_foreign",    { WI32, WI32, WI32, WI32,
                                                                          WI32, WI32                }),
        functionImport ( hook_api::state_set,       "state_set",        { WI32, WI32, WI32, WI32    }),
        functionImport ( hook_api::slot_set,        "slot_set",         { WI32, WI32, WI32, WI32    }),
        functionImport ( hook_api::slot_clear,      "slot_clear",       { WI32                      }),
        functionImport ( hook_api::slot_field_txt,  "slot_field_txt",   { WI32, WI32, WI32, WI32    }),
        functionImport ( hook_api::slot_field,      "slot_field",       { WI32, WI32, WI32, WI32    }),
        functionImport ( hook_api::slot_id,         "slot_id",          { WI32                      }),
        functionImport ( hook_api::slot_type,       "slot_type",        { WI32                      }),
        functionImport ( hook_api::trace,           "trace",            { WI32, WI32, WI32          }),
        functionImport ( hook_api::trace_slot,      "trace_slot",       { WI32                      }),
        functionImport ( hook_api::trace_num,       "trace_num",        { WI32, WI32, WI64          })
    };


#define HOOK_SETUP()\
    hook::HookContext& hookCtx = *((hook::HookContext*) wasmer_instance_context_data_get( wasm_ctx ));\
    [[maybe_unused]] ApplyContext& applyCtx = hookCtx.applyCtx;\
    [[maybe_unused]] auto& view = applyCtx.view();\
    [[maybe_unused]] auto j = applyCtx.app.journal("View");\
    const wasmer_memory_t* memory_ctx = wasmer_instance_context_memory( wasm_ctx, 0 );\
    [[maybe_unused]] uint8_t* memory = wasmer_memory_data( memory_ctx );\
    [[maybe_unused]] const uint64_t memory_length = wasmer_memory_data_length ( memory_ctx );


#define WRITE_WASM_MEMORY_AND_RETURN(guest_dst_ptr, guest_dst_len,\
        host_src_ptr, host_src_len, host_memory_ptr, guest_memory_length)\
    {int bytes_to_write = std::min(static_cast<int>(host_src_len), static_cast<int>(guest_dst_len));\
    if (guest_dst_ptr + bytes_to_write > guest_memory_length) {\
        JLOG(j.trace())\
            << "Hook: " << __func__ << " tried to retreive blob of " << host_src_len <<\
        " bytes past end of wasm memory";\
        return OUT_OF_BOUNDS;\
    }\
    ::memcpy(host_memory_ptr + guest_dst_ptr, host_src_ptr, bytes_to_write);\
    return bytes_to_write;}

// ptr = pointer inside the wasm memory space
#define NOT_IN_BOUNDS(ptr, len, memory_length)\
    (ptr > memory_length || \
     static_cast<uint64_t>(ptr) + static_cast<uint64_t>(len) > static_cast<uint64_t>(memory_length))


#endif

}


#ifndef RIPPLE_HOOK_H_TEMPLATES
#define RIPPLE_HOOK_H_TEMPLATES
// templates must be defined in the same file they are declared in, otherwise this would go in impl/Hook.cpp
template <typename F>
wasmer_import_t hook::functionImport (
        F func, std::string_view call_name, std::initializer_list<wasmer_value_tag> func_params, int ret_type )
{
    return
    {   .module_name = { .bytes = (const uint8_t *) "env", .bytes_len = 3 },
        .import_name = { .bytes = (const uint8_t *) call_name.data(), .bytes_len = call_name.size() },
        .tag = wasmer_import_export_kind::WASM_FUNCTION,
        .value = { .func =
            wasmer_import_func_new(
                reinterpret_cast<void (*)(void*)>(func),
                std::begin( func_params ),
                func_params.size(),
                (ret_type == 0 ? NULL : ( ret_type == 1 ? std::begin ( { WI32 } ) : std::begin( { WI64 } ))),
                (ret_type == 0 ? 0 : 1)
            )
        }
    };
}
#endif
