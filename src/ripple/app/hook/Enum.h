#include <set>
#include <string>
#ifndef HOOKENUM_INCLUDED
#define HOOKENUM_INCLUDED 1
namespace ripple
{
    enum HookSetOperation : int8_t
    {
        hsoINVALID  = -1,
        hsoNOOP     = 0,
        hsoCREATE   = 1,
        hsoINSTALL  = 2,
        hsoDELETE   = 3,
        hsoNSDELETE = 4,
        hsoUPDATE   = 5
    };
        
    enum HookSetFlags : uint8_t
    {
        hsfOVERRIDE = 0b00000001U,       // override or delete hook
        hsfNSDELETE = 0b00000010U,       // delete namespace
        hsfCOLLECT  = 0b00000100U,       // allow collect calls on this hook
    };
}

namespace hook
{
    enum TSHFlags : uint8_t
    {
        tshNONE                = 0b000,
        tshROLLBACK            = 0b001,
        tshCOLLECT             = 0b010,
        tshMIXED               = 0b100,
    };


    namespace log
    {
        /*
         * Hook log-codes are not necessarily errors. Each type of Hook log line contains a code in
         * round parens like so:
         *      HookSet(5)[rAcc...]: message
         * The log-code gives an external tool an easy way to handle and report the status of a hook
         * to end users and developers.
         */
        enum hook_log_code : uint16_t
        {
            /* HookSet log-codes */
            AMENDMENT_DISABLED   = 1,  // attempt to HookSet when amendment is not yet enabled.                       
            API_ILLEGAL          = 2,  // HookSet object contained HookApiVersion for existing HookDefinition         
            API_INVALID          = 3,  // HookSet object contained HookApiVersion for unrecognised hook API           
            API_MISSING          = 4,  // HookSet object did not contain HookApiVersion but should have               
            BLOCK_ILLEGAL        = 5,  // a block end instruction moves execution below depth 0 {{}}`}` <= like this  
            CALL_ILLEGAL         = 6,  // wasm tries to call a non-whitelisted function                               
            CALL_INDIRECT        = 7,  // wasm used call indirect instruction which is disallowed                     
            CREATE_FLAG          = 8,  // create operation requires hsfOVERRIDE flag                                  
            DELETE_FIELD         = 9,  //                                                                             
            DELETE_FLAG          = 10,  // delete operation requires hsfOVERRIDE flag                                  
            DELETE_NOTHING       = 11,  // delete operation would delete nothing                                       
            EXPORTS_MISSING      = 12,  // hook did not export *any* functions (should be cbak, hook)                  
            EXPORT_CBAK_FUNC     = 13,  // hook did not export correct func def int64_t cbak(uint32_t)                 
            EXPORT_HOOK_FUNC     = 14,  // hook did not export correct func def int64_t hook(uint32_t)                 
            EXPORT_MISSING       = 15,  // distinct from export*S*_missing, either hook or cbak is missing             
            FLAGS_INVALID        = 16,  // HookSet flags were invalid for specified operation                          
            FUNCS_MISSING        = 17,  // hook did not include function code for any functions                        
            FUNC_PARAM_INVALID   = 18,  // parameter types may only be i32 i64 u32 u64                                 
            FUNC_RETURN_COUNT    = 19,  // a function type is defined in the wasm which returns > 1 return value       
            FUNC_RETURN_INVALID  = 20,  // a function type does not return i32 i64 u32 or u64                          
            FUNC_TYPELESS        = 21,  // hook defined hook/cbak but their type is not defined in wasm                
            FUNC_TYPE_INVALID    = 22,  // malformed and illegal wasm in the func type section                         
            GRANTS_EMPTY         = 23,  // HookSet object contained an empty grants array (you should remove it)       
            GRANTS_EXCESS        = 24,  // HookSet object cotnained a grants array with too many grants                
            GRANTS_FIELD         = 25,  // HookSet object contained a grant without Authorize or HookHash              
            GRANTS_ILLEGAL       = 26,  // Hookset object contained grants array which contained a non Grant object    
            GUARD_IMPORT         = 27,  // guard import is missing                                                     
            GUARD_MISSING        = 28,  // guard call missing at top of loop                                           
            GUARD_PARAMETERS     = 29,  // guard called but did not use constant expressions for params                
            HASH_OR_CODE         = 30,  // HookSet object can contain only one of CreateCode and HookHash              
            HOOKON_MISSING       = 31,  // HookSet object did not contain HookOn but should have                       
            HOOKS_ARRAY_BAD      = 32,  // attempt to HookSet with a Hooks array containing a non-Hook obj             
            HOOKS_ARRAY_BLANK    = 33,  // all hook set objs were blank                                                
            HOOKS_ARRAY_EMPTY    = 34,  // attempt to HookSet with an empty Hooks array                                
            HOOKS_ARRAY_MISSING  = 35,  // attempt to HookSet without a Hooks array                                    
            HOOKS_ARRAY_TOO_BIG  = 36,  // attempt to HookSet with a Hooks array beyond the chain size limit           
            HOOK_ADD             = 37,  // Informational: adding ltHook to directory                                   
            HOOK_DEF_MISSING     = 38,  // attempt to reference a hook definition (by hash) that is not on ledger      
            HOOK_DELETE          = 39,  // unable to delete ltHook from owner                                          
            HOOK_INVALID_FIELD   = 40,  // HookSetObj contained an illegal/unexpected field                            
            HOOK_PARAMS_COUNT    = 41,  // hookset obj would create too many hook parameters                           
            HOOK_PARAM_SIZE      = 42,  // hookset obj sets a parameter or value that exceeds max allowable size       
            IMPORTS_MISSING      = 43,  // hook must import guard, and accept/rollback                                 
            IMPORT_ILLEGAL       = 44,  // attempted import of a non-whitelisted function                              
            IMPORT_MODULE_BAD    = 45,  // hook attempted to specify no or a bad import module                         
            IMPORT_MODULE_ENV    = 46,  // hook attempted to specify import module not named env                       
            IMPORT_NAME_BAD      = 47,  // import name was too short or too long                                       
            INSTALL_FLAG         = 48,  // install operation requires hsoOVERRIDE                                      
            INSTALL_MISSING      = 49,  // install operation specifies hookhash which doesn't exist on the ledger      
            INSTRUCTION_COUNT    = 50,  // worst case execution instruction count as computed by HookSet               
            INSTRUCTION_EXCESS   = 51,  // worst case execution instruction count was too large                        
            MEMORY_GROW          = 52,  // memory.grow instruction is present but disallowed                           
            NAMESPACE_MISSING    = 53,  // HookSet object lacked HookNamespace                                         
            NSDELETE             = 54,  // Informational: a namespace is being deleted                                 
            NSDELETE_ACCOUNT     = 55,  // nsdelete tried to delete ns from a non-existing account                     
            NSDELETE_COUNT       = 56,  // namespace state count less than 0 / overflow                                
            NSDELETE_DIR         = 57,  // could not delete directory node in ledger                                   
            NSDELETE_DIRECTORY   = 58,  // nsdelete operation failed to delete ns directory                            
            NSDELETE_DIR_ENTRY   = 59,  // nsdelete operation failed due to bad entry in ns directory                  
            NSDELETE_ENTRY       = 60,  // nsdelete operation failed due to missing hook state entry                   
            NSDELETE_FIELD       = 61,                                                                                 
            NSDELETE_FLAGS       = 62,                                                                                 
            NSDELETE_NONSTATE    = 63,  // nsdelete operation failed due to the presence of a non-hookstate obj        
            NSDELETE_NOTHING     = 64,  // hsfNSDELETE provided but nothing to delete                                  
            OPERATION_INVALID    = 65,  // could not deduce an operation from the provided hookset obj                 
            OVERRIDE_MISSING     = 66,  // HookSet object was trying to update or delete a hook but lacked hsfOVERRIDE 
            PARAMETERS_FIELD     = 67,  // HookParameters contained a HookParameter with an invalid key in it          
            PARAMETERS_ILLEGAL   = 68,  // HookParameters contained something other than a HookParameter               
            PARAMETERS_NAME      = 69,  // HookParameters contained a HookParameter which lacked ParameterName field   
            PARAM_HOOK_CBAK      = 70,  // hook and cbak must take exactly one u32 parameter                           
            RETURN_HOOK_CBAK     = 71,  // hook and cbak must retunr i64                                               
            SHORT_HOOK           = 72,  // web assembly byte code ended abruptly                                       
            TYPE_INVALID         = 73,  // malformed and illegal wasm specifying an illegal local var type             
            WASM_BAD_MAGIC       = 74,  // wasm magic number missing or not wasm                                       
            WASM_INVALID         = 75,  // set hook operation would set invalid wasm                                   
            WASM_PARSE_LOOP      = 76,  // wasm section parsing resulted in an infinite loop                           
            WASM_SMOKE_TEST      = 77,  // Informational: first attempt to load wasm into wasm runtime                 
            WASM_TEST_FAILURE    = 78,  // the smoke test failed                                                       
            WASM_TOO_BIG         = 79,  // set hook would exceed maximum hook size                                     
            WASM_TOO_SMALL       = 80,                                                                                 
            WASM_VALIDATION      = 81,  // a generic error while parsing wasm, usually leb128 overflow
            HOOK_CBAK_DIFF_TYPES = 82,  // hook and cbak function definitions were different
            PARAMETERS_NAME_REPEATED = 83,
            NESTING_LIMIT = 84,         // the hook nested blocks/loops/ifs beyond 16 levels
            SECTIONS_OUT_OF_SEQUENCE = 85,  // the wasm contained sections out of sequence
            CUSTOM_SECTION_DISALLOWED = 86, // the wasm contained a custom section (id=0)
            // RH NOTE: only HookSet msgs got log codes, possibly all Hook log lines should get a code? 
        };
    };
};

namespace hook_api
{

    namespace keylet_code
    {
        enum keylet_code : uint32_t
        {
            HOOK = 1,
            HOOK_STATE = 2,
            ACCOUNT = 3,
            AMENDMENTS = 4,
            CHILD = 5,
            SKIP = 6,
            FEES = 7,
            NEGATIVE_UNL = 8,
            LINE = 9,
            OFFER = 10,
            QUALITY = 11,
            EMITTED_DIR = 12,
            TICKET = 13,
            SIGNERS = 14,
            CHECK = 15,
            DEPOSIT_PREAUTH = 16,
            UNCHECKED = 17,
            OWNER_DIR = 18,
            PAGE = 19,
            ESCROW = 20,
            PAYCHAN = 21,
            EMITTED_TXN = 22,
            NFT_OFFER = 23,
            HOOK_DEFINITION = 24,
            LAST_KLTYPE = HOOK_DEFINITION
        };
    }

    namespace compare_mode {
        enum compare_mode : uint32_t
        {
            EQUAL = 1,
            LESS = 2,
            GREATER = 4
        };
    }

    enum hook_return_code : int64_t
    {
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
        PARSE_ERROR = -18,              // hook asked hookapi to parse something the contents of which was invalid
        RC_ROLLBACK = -19,              // hook should terminate due to a rollback() call
        RC_ACCEPT = -20,                // hook should temrinate due to an accept() call
        NO_SUCH_KEYLET = -21,           // invalid keylet or keylet type
        NOT_AN_ARRAY = -22,             // if a count of an sle is requested but its not STI_ARRAY
        NOT_AN_OBJECT = -23,            // if a subfield is requested from something that isn't an object
        INVALID_FLOAT = -10024,         // specially selected value that will never be a valid exponent
        DIVISION_BY_ZERO = -25,
        MANTISSA_OVERSIZED = -26,
        MANTISSA_UNDERSIZED = -27,
        EXPONENT_OVERSIZED = -28,
        EXPONENT_UNDERSIZED = -29,
        XFL_OVERFLOW = -30,                 // if an operation with a float results in an overflow
        NOT_IOU_AMOUNT = -31,
        NOT_AN_AMOUNT = -32,
        CANT_RETURN_NEGATIVE = -33,
        NOT_AUTHORIZED = -34,
        PREVIOUS_FAILURE_PREVENTS_RETRY = -35,
        TOO_MANY_PARAMS = -36,
        INVALID_TXN = -37,
        RESERVE_INSUFFICIENT = -38,     // setting a new state object would exceed account reserve 
        COMPLEX_NOT_SUPPORTED = -39,
        DOES_NOT_MATCH = -40,           // two keylets were required to be the same type but werent
        INVALID_KEY = -41,              // user supplied key was not valid
        NOT_A_STRING = -42,             // nul terminator missing from a string argument
        MEM_OVERLAP  = -43,             // one or more specified buffers are the same memory
        TOO_MANY_STATE_MODIFICATIONS = -44, // more than 5000 modified state entires in the combined hook chains
    };

    enum ExitType : uint8_t
    {
        UNSET = 0,
        WASM_ERROR = 1,
        ROLLBACK = 2,
        ACCEPT = 3,
    };

    const uint16_t max_state_modifications = 5000;
    const uint8_t max_slots = 255;
    const uint8_t max_nonce = 255;
    const uint8_t max_emit = 255;
    const uint8_t max_params = 16;
    const double fee_base_multiplier = 1.1f;

    // RH NOTE: Find descriptions of api functions in ./impl/applyHook.cpp and hookapi.h (include for hooks)
    // this is a map of the api name to its return code (vec[0] and its parameters vec[>0]) as wasm type codes
    static const std::map<std::string, std::vector<uint8_t>> import_whitelist
    {
        {"_g",{0x7FU,0x7FU,0x7FU}},
        {"accept",{0x7EU,0x7FU,0x7FU,0x7EU}},
        {"rollback",{0x7EU,0x7FU,0x7FU,0x7EU}},
        {"util_raddr",{0x7EU,0x7FU,0x7FU,0x7FU,0x7FU}},
        {"util_accid",{0x7EU,0x7FU,0x7FU,0x7FU,0x7FU}},
        {"util_verify",{0x7EU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU}},
        {"sto_validate",{0x7EU,0x7FU,0x7FU}},
        {"sto_subfield",{0x7EU,0x7FU,0x7FU,0x7FU}},
        {"sto_subarray",{0x7EU,0x7FU,0x7FU,0x7FU}},
        {"sto_emplace",{0x7EU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU}},
        {"sto_erase",{0x7EU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU}},
        {"util_sha512h",{0x7EU,0x7FU,0x7FU,0x7FU,0x7FU}},
        {"util_keylet",{0x7EU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU}},
        {"etxn_burden",{0x7EU}},
        {"etxn_details",{0x7EU,0x7FU,0x7FU}},
        {"etxn_fee_base",{0x7EU,0x7FU,0x7FU}},
        {"etxn_reserve",{0x7EU,0x7FU}},
        {"etxn_generation",{0x7EU}},
        {"etxn_nonce",{0x7EU,0x7FU,0x7FU}},
        {"emit",{0x7EU,0x7FU,0x7FU,0x7FU,0x7FU}},
        {"float_set",{0x7EU,0x7FU,0x7EU}},
        {"float_multiply",{0x7EU,0x7EU,0x7EU}},
        {"float_mulratio",{0x7EU,0x7EU,0x7FU,0x7FU,0x7FU}},
        {"float_negate",{0x7EU,0x7EU}},
        {"float_compare",{0x7EU,0x7EU,0x7EU,0x7FU}},
        {"float_sum",{0x7EU,0x7EU,0x7EU}},
        {"float_sto",{0x7EU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU,0x7EU,0x7FU}},
        {"float_sto_set",{0x7EU,0x7FU,0x7FU}},
        {"float_invert",{0x7EU,0x7EU}},
        {"float_divide",{0x7EU,0x7EU,0x7EU}},
        {"float_one",{0x7EU}},
        {"float_mantissa",{0x7EU,0x7EU}},
        {"float_sign",{0x7EU,0x7EU}},
        {"float_int",{0x7EU,0x7EU,0x7FU,0x7FU}},
        {"float_log",{0x7EU,0x7EU}},
        {"float_root",{0x7EU,0x7EU,0x7FU}},
        {"hook_account",{0x7EU,0x7FU,0x7FU}},
        {"hook_hash",{0x7EU,0x7FU,0x7FU,0x7FU}},
        {"fee_base",{0x7EU}},
        {"ledger_seq",{0x7EU}},
        {"ledger_last_time",{0x7EU}},
        {"ledger_last_hash",{0x7EU,0x7FU,0x7FU}},
        {"ledger_nonce",{0x7EU,0x7FU,0x7FU}},
        {"ledger_keylet",{0x7EU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU}},
        {"hook_param_set",{0x7EU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU}},
        {"hook_param",{0x7EU,0x7FU,0x7FU,0x7FU,0x7FU}},
        {"hook_again",{0x7EU}},
        {"hook_skip",{0x7EU,0x7FU,0x7FU,0x7FU}},
        {"hook_pos",{0x7EU}},
        {"slot",{0x7EU,0x7FU,0x7FU,0x7FU}},
        {"slot_clear",{0x7EU,0x7FU}},
        {"slot_count",{0x7EU,0x7FU}},
        {"slot_set",{0x7EU,0x7FU,0x7FU,0x7FU}},
        {"slot_size",{0x7EU,0x7FU}},
        {"slot_subarray",{0x7EU,0x7FU,0x7FU,0x7FU}},
        {"slot_subfield",{0x7EU,0x7FU,0x7FU,0x7FU}},
        {"slot_type",{0x7EU,0x7FU,0x7FU}},
        {"slot_float",{0x7EU,0x7FU}},
        {"state_set",{0x7EU,0x7FU,0x7FU,0x7FU,0x7FU}},
        {"state_foreign_set",{0x7EU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU}},
        {"state",{0x7EU,0x7FU,0x7FU,0x7FU,0x7FU}},
        {"state_foreign",{0x7EU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU}},
        {"trace",{0x7EU,0x7FU,0x7FU,0x7FU,0x7FU,0x7FU}},
        {"trace_num",{0x7EU,0x7FU,0x7FU,0x7EU}},
        {"trace_float",{0x7EU,0x7FU,0x7FU,0x7EU}},
        {"otxn_burden",{0x7EU}},
        {"otxn_field",{0x7EU,0x7FU,0x7FU,0x7FU}},
        {"otxn_generation",{0x7EU}},
        {"otxn_id",{0x7EU,0x7FU,0x7FU,0x7FU}},
        {"otxn_type",{0x7EU}},
        {"otxn_slot",{0x7EU,0x7FU}},
        {"otxn_param",{0x7EU,0x7FU,0x7FU,0x7FU,0x7FU}},
        {"meta_slot",{0x7EU,0x7FU}}        
    };
};
#endif
