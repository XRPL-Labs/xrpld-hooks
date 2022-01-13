/**
 * RH NOTE:
 * This file contains macros for converting the hook api definitions into the currently used wasm runtime.
 * Web assembly runtimes are more or less fungible, and at time of writing hooks has moved to WasmEdge from SSVM
 * and before that from wasmer. 
 * After the first move it was decided there should be a relatively static interface for the definition and
 * programming of the hook api itself, with the runtime-specific behaviour hidden away by templates or macros.
 * Macros are more expressive and can themselves include templates so macros were then used.
 */

#define LPAREN (
#define LPAREN (
#define RPAREN )
#define COMMA ,
#define EXPAND(...) __VA_ARGS__
#define CAT(a, ...) PRIMITIVE_CAT(a, __VA_ARGS__)
#define CAT2(L, R) CAT2_(L, R)
#define CAT2_(L, R) L ## R
#define PRIMITIVE_CAT(a, ...) a ## __VA_ARGS__
#define EMPTY()
#define DEFER(id) id EMPTY()
#define OBSTRUCT(...) __VA_ARGS__ DEFER(EMPTY)()
#define VA_NARGS_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N
#define VA_NARGS(__drop, ...) VA_NARGS_IMPL(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)
#define FIRST(a, b) a
#define SECOND(a, b) b
#define STRIP_TYPES(...) FOR_VARS(SECOND, 0, __VA_ARGS__) 

#define DELIM_0 ,
#define DELIM_1 
#define DELIM_2 ;
#define DELIM(S) DELIM_##S

#define FOR_VAR_1(T, D) SEP(T, D)
#define FOR_VAR_2(T, S, a, b)    FOR_VAR_1(T, a) DELIM(S) FOR_VAR_1(T, b)
#define FOR_VAR_3(T, S, a, ...)  FOR_VAR_1(T, a) DELIM(S) FOR_VAR_2(T, S, __VA_ARGS__)
#define FOR_VAR_4(T, S, a, ...)  FOR_VAR_1(T, a) DELIM(S) FOR_VAR_3(T, S, __VA_ARGS__)
#define FOR_VAR_5(T, S, a, ...)  FOR_VAR_1(T, a) DELIM(S) FOR_VAR_4(T, S, __VA_ARGS__)
#define FOR_VAR_6(T, S, a, ...)  FOR_VAR_1(T, a) DELIM(S) FOR_VAR_5(T, S, __VA_ARGS__)
#define FOR_VAR_7(T, S, a, ...)  FOR_VAR_1(T, a) DELIM(S) FOR_VAR_6(T, S, __VA_ARGS__)
#define FOR_VAR_8(T, S, a, ...)  FOR_VAR_1(T, a) DELIM(S) FOR_VAR_7(T, S, __VA_ARGS__)
#define FOR_VAR_9(T, S, a, ...)  FOR_VAR_1(T, a) DELIM(S) FOR_VAR_8(T, S, __VA_ARGS__)
#define FOR_VAR_10(T, S, a, ...) FOR_VAR_1(T, a) DELIM(S) FOR_VAR_9(T, S, __VA_ARGS__)
#define FOR_VARS(T, S, ...)\
    DEFER(CAT(FOR_VAR_,VA_NARGS(NULL, __VA_ARGS__))CAT(LPAREN T COMMA S COMMA OBSTRUCT(__VA_ARGS__) RPAREN))

#define SEP(OP, D) EXPAND(OP CAT2(SEP_, D) RPAREN)
#define SEP_uint32_t    LPAREN uint32_t COMMA
#define SEP_int32_t     LPAREN int32_t COMMA
#define SEP_uint64_t    LPAREN uint64_t COMMA
#define SEP_int64_t     LPAREN int64_t COMMA

#define VAL_uint32_t    WasmEdge_ValueGetI32(in[_stack++])
#define VAL_int32_t     WasmEdge_ValueGetI32(in[_stack++])
#define VAL_uint64_t    WasmEdge_ValueGetI64(in[_stack++])
#define VAL_int64_t     WasmEdge_ValueGetI64(in[_stack++])

#define VAR_ASSIGN(T, V)\
    T V = CAT(VAL_ ##T)

#define RET_uint32_t(return_code)   WasmEdge_ValueGenI32(return_code)
#define RET_int32_t(return_code)    WasmEdge_ValueGenI32(return_code)
#define RET_uint64_t(return_code)   WasmEdge_ValueGenI64(return_code)
#define RET_int64_t(return_code)    WasmEdge_ValueGenI64(return_code)

#define RET_ASSIGN(T, return_code)\
    CAT2(RET_,T(return_code))

#define TYP_uint32_t WasmEdge_ValType_I32
#define TYP_int32_t WasmEdge_ValType_I32
#define TYP_uint64_t WasmEdge_ValType_I64
#define TYP_int64_t WasmEdge_ValType_I64

#define WASM_VAL_TYPE(T, b)\
    CAT2(TYP_,T)

#define DECLARE_HOOK_FUNCTION(R, F, ...)\
    WasmEdge_Result WasmFunction##F(\
        void *data_ptr, WasmEdge_MemoryInstanceContext *memCtx,\
        const WasmEdge_Value *in, WasmEdge_Value *out)\
    {\
        int _stack = 0;\
        FOR_VARS(VAR_ASSIGN, 2, __VA_ARGS__);\
        hook::HookContext* hookCtx = reinterpret_cast<hook::HookContext*>(data_ptr);\
        R return_code = hook_api::F(*hookCtx, *memCtx, STRIP_TYPES(__VA_ARGS__));\
        if (return_code == RC_ROLLBACK || return_code == RC_ACCEPT)\
            return WasmEdge_Result_Terminate;\
        out[0] = RET_ASSIGN(R, return_code);\
        return WasmEdge_Result_Success;\
    };\
    WasmEdge_FunctionTypeContext* WasmFunctionType##F = NULL;\
    {\
        enum WasmEdge_ValType r =  { WASM_VAL_TYPE(R, dummy) };\
        enum WasmEdge_ValType p =  { FOR_VARS(WASM_VAL_TYPE, 0, __VA_ARGS__) };\
        WasmFunctionType##F = WasmEdge_FunctionTypeCreate(p, VA_NARGS(NULL, __VA_ARGS__), r, 1);\
    };\
    WasmEdge_String WasmFunctionName##F = WasmEdge_StringCreateByCString(#F);


#define DECLARE_HOOK_FUNCNARG(R, F)\
    WasmEdge_Result WasmFunction_##F(\
        void *data_ptr, WasmEdge_MemoryInstanceContext *memCtx,\
        const WasmEdge_Value *in, WasmEdge_Value *out)\
    {\
        hook::HookContext* hookCtx = reinterpret_cast<hook::HookContext*>(data_ptr);\
        R return_code = hook_api::F(*hookCtx, *memCtx);\
        if (return_code == RC_ROLLBACK || return_code == RC_ACCEPT)\
            return WasmEdge_Result_Terminate;\
        Out[0] = CAT2(RET_,R(return_code));\
        return WasmEdge_Result_Success;\
    };\
    WasmEdge_FunctionTypeContext* WasmFunctionType##F = NULL;\
    {\
        enum WasmEdge_ValType r =  { WASM_VAL_TYPE(R, dummy) };\
        enum WasmEdge_ValType p =  { };\
        WasmFunctionType##F = WasmEdge_FunctionTypeCreate(p, 0, r, 1);\
    }\
    WasmEdge_String WasmFunctionName##F = WasmEdge_StringCreateByCString(#F);

#define DEFINE_HOOK_FUNCTION(R, F, ...)\
        R hook_api::F(hook::HookContext& hookCtx, WasmEdge_MemoryInstanceContext& memoryCtx, __VA_ARGS__)

#define DEFINE_HOOK_FUNCNARG(R, F)\
        R hook_api::F(hook::HookContext& hookCtx, WasmEdge_MemoryInstanceContext& memoryCtx)

