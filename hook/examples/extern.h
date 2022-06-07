// For documentation please see: https://xrpl-hooks.readme.io/reference/
// Generated using generate_extern.sh
#include <stdint.h>
#ifndef HOOK_EXTERN

extern int32_t 
__attribute__((noduplicate))
_g(
    uint32_t guard_id,
    uint32_t maxiter
);

extern int64_t 
accept(
    uint32_t read_ptr,
    uint32_t read_len,
    int64_t error_code
);

extern int64_t 
emit(
    uint32_t write_ptr,
    uint32_t write_len,
    uint32_t read_ptr,
    uint32_t read_len
);

extern int64_t 
etxn_burden (
    void
);

extern int64_t 
etxn_details(
    uint32_t write_ptr,
    uint32_t write_len
);

extern int64_t 
etxn_fee_base(
    uint32_t read_ptr,
    uint32_t read_len
);

extern int64_t 
etxn_generation (
    void
);

extern int64_t 
etxn_nonce(
    uint32_t write_ptr,
    uint32_t write_len
);

extern int64_t 
etxn_reserve(
    uint32_t count
);

extern int64_t 
fee_base (
    void
);

extern int64_t 
float_compare(
    int64_t float1,
    int64_t float2,
    uint32_t mode
);

extern int64_t 
float_divide(
    int64_t float1,
    int64_t float2
);

extern int64_t 
float_exponent(
    int64_t float1
);

extern int64_t 
float_exponent_set(
    int64_t float1,
    int32_t exponent
);

extern int64_t 
float_int(
    int64_t float1,
    uint32_t decimal_places,
    uint32_t abs
);

extern int64_t 
float_invert(
    int64_t float1
);

extern int64_t 
float_log(
    int64_t float1
);

extern int64_t 
float_mantissa(
    int64_t float1
);

extern int64_t 
float_mantissa_set(
    int64_t float1,
    int64_t mantissa
);

extern int64_t 
float_mulratio(
    int64_t float1,
    uint32_t round_up,
    uint32_t numerator,
    uint32_t denominator
);

extern int64_t 
float_multiply(
    int64_t float1,
    int64_t float2
);

extern int64_t 
float_negate(
    int64_t float1
);

extern int64_t 
float_one (
    void
);

extern int64_t 
float_root(
    int64_t float1,
    uint32_t n
);

extern int64_t 
float_set(
    int32_t exponent,
    int64_t mantissa
);

extern int64_t 
float_sign(
    int64_t float1
);

extern int64_t 
float_sign_set(
    int64_t float1,
    uint32_t negative
);

extern int64_t 
float_sto(
    uint32_t write_ptr,
    uint32_t write_len,
    uint32_t cread_ptr,
    uint32_t cread_len,
    uint32_t iread_ptr,
    uint32_t iread_len,
    int64_t float1,
    uint32_t field_code
);

extern int64_t 
float_sto_set(
    uint32_t read_ptr,
    uint32_t read_len
);

extern int64_t 
float_sum(
    int64_t float1,
    int64_t float2
);

extern int64_t 
hook_account(
    uint32_t write_ptr,
    uint32_t write_len
);

extern int64_t 
hook_again (
    void
);

extern int64_t 
hook_hash(
    uint32_t write_ptr,
    uint32_t write_len,
    int32_t hook_no
);

extern int64_t 
hook_param(
    uint32_t write_ptr,
    uint32_t write_len,
    uint32_t read_ptr,
    uint32_t read_len
);

extern int64_t 
hook_param_set(
    uint32_t read_ptr,
    uint32_t read_len,
    uint32_t kread_ptr,
    uint32_t kread_len,
    uint32_t hread_ptr,
    uint32_t hread_len
);

extern int64_t 
hook_pos (
    void
);

extern int64_t 
hook_skip(
    uint32_t read_ptr,
    uint32_t read_len,
    uint32_t flags
);

extern int64_t 
ledger_keylet(
    uint32_t write_ptr,
    uint32_t write_len,
    uint32_t lread_ptr,
    uint32_t lread_len,
    uint32_t hread_ptr,
    uint32_t hread_len
);

extern int64_t 
ledger_last_hash(
    uint32_t write_ptr,
    uint32_t write_len
);

extern int64_t 
ledger_last_time (
    void
);

extern int64_t 
ledger_nonce(
    uint32_t write_ptr,
    uint32_t write_len
);

extern int64_t 
ledger_seq (
    void
);

extern int64_t 
meta_slot(
    uint32_t slot_no
);

extern int64_t 
otxn_burden (
    void
);

extern int64_t 
otxn_field(
    uint32_t write_ptr,
    uint32_t write_len,
    uint32_t field_id
);

extern int64_t 
otxn_field_txt(
    uint32_t write_ptr,
    uint32_t write_len,
    uint32_t field_id
);

extern int64_t 
otxn_generation (
    void
);

extern int64_t 
otxn_id(
    uint32_t write_ptr,
    uint32_t write_len,
    uint32_t flags
);

extern int64_t 
otxn_slot(
    uint32_t slot_no
);

extern int64_t 
otxn_type (
    void
);

extern int64_t 
rollback(
    uint32_t read_ptr,
    uint32_t read_len,
    int64_t error_code
);

extern int64_t 
slot(
    uint32_t write_ptr,
    uint32_t write_len,
    uint32_t slot
);

extern int64_t 
slot_clear(
    uint32_t slot
);

extern int64_t 
slot_count(
    uint32_t slot
);

extern int64_t 
slot_float(
    uint32_t slot_no
);

extern int64_t 
slot_id(
    uint32_t write_ptr,
    uint32_t write_len,
    uint32_t slot
);

extern int64_t 
slot_set(
    uint32_t read_ptr,
    uint32_t read_len,
    int32_t slot
);

extern int64_t 
slot_size(
    uint32_t slot
);

extern int64_t 
slot_subarray(
    uint32_t parent_slot,
    uint32_t array_id,
    uint32_t new_slot
);

extern int64_t 
slot_subfield(
    uint32_t parent_slot,
    uint32_t field_id,
    uint32_t new_slot
);

extern int64_t 
slot_type(
    uint32_t slot_no,
    uint32_t flags
);

extern int64_t 
state(
    uint32_t write_ptr,
    uint32_t write_len,
    uint32_t kread_ptr,
    uint32_t kread_len
);

extern int64_t 
state_foreign(
    uint32_t write_ptr,
    uint32_t write_len,
    uint32_t kread_ptr,
    uint32_t kread_len,
    uint32_t nread_ptr,
    uint32_t nread_len,
    uint32_t aread_ptr,
    uint32_t aread_len
);

extern int64_t 
state_foreign_set(
    uint32_t read_ptr,
    uint32_t read_len,
    uint32_t kread_ptr,
    uint32_t kread_len,
    uint32_t nread_ptr,
    uint32_t nread_len,
    uint32_t aread_ptr,
    uint32_t aread_len
);

extern int64_t 
state_set(
    uint32_t read_ptr,
    uint32_t read_len,
    uint32_t kread_ptr,
    uint32_t kread_len
);

extern int64_t 
sto_emplace(
    uint32_t write_ptr,
    uint32_t write_len,
    uint32_t sread_ptr,
    uint32_t sread_len,
    uint32_t fread_ptr,
    uint32_t fread_len,
    uint32_t field_id
);

extern int64_t 
sto_erase(
    uint32_t write_ptr,
    uint32_t write_len,
    uint32_t read_ptr,
    uint32_t read_len,
    uint32_t field_id
);

extern int64_t 
sto_subarray(
    uint32_t read_ptr,
    uint32_t read_len,
    uint32_t array_id
);

extern int64_t 
sto_subfield(
    uint32_t read_ptr,
    uint32_t read_len,
    uint32_t field_id
);

extern int64_t 
sto_validate(
    uint32_t tread_ptr,
    uint32_t tread_len
);

extern int64_t 
trace(
    uint32_t mread_ptr,
    uint32_t mread_len,
    uint32_t dread_ptr,
    uint32_t dread_len,
    uint32_t as_hex
);

extern int64_t 
trace_float(
    uint32_t read_ptr,
    uint32_t read_len,
    int64_t float1
);

extern int64_t 
trace_num(
    uint32_t read_ptr,
    uint32_t read_len,
    int64_t number
);

extern int64_t 
trace_slot(
    uint32_t read_ptr,
    uint32_t read_len,
    uint32_t slot
);

extern int64_t 
util_accid(
    uint32_t write_ptr,
    uint32_t write_len,
    uint32_t read_ptr,
    uint32_t read_len
);

extern int64_t 
util_keylet(
    uint32_t write_ptr,
    uint32_t write_len,
    uint32_t keylet_type,
    uint32_t a,
    uint32_t b,
    uint32_t c,
    uint32_t d,
    uint32_t e,
    uint32_t f
);

extern int64_t 
util_raddr(
    uint32_t write_ptr,
    uint32_t write_len,
    uint32_t read_ptr,
    uint32_t read_len
);

extern int64_t 
util_sha512h(
    uint32_t write_ptr,
    uint32_t write_len,
    uint32_t read_ptr,
    uint32_t read_len
);

extern int64_t 
util_verify(
    uint32_t dread_ptr,
    uint32_t dread_len,
    uint32_t sread_ptr,
    uint32_t sread_len,
    uint32_t kread_ptr,
    uint32_t kread_len
);
#define HOOK_EXTERN
#endif //HOOK_EXTERN
