/**
 * Hook API include for Webassembly XRPLD Hooks
 *
 * Note to the reader:
 * This include defines two types of things: external functions and macros
 * Functions are used sparingly because a non-inlining compiler may produce
 * undesirable output.
 */

#ifndef HOOKAPI_INCLUDED
#define HOOKAPI_INCLUDED 1
#include <stdint.h>

int64_t hook(int64_t reserved) __attribute__((used));
int64_t cbak(int64_t reserved) __attribute__((used));

extern int32_t _g                  (uint32_t id, uint32_t maxiter);
extern int64_t accept              (uint32_t read_ptr,  uint32_t read_len,   int32_t error_code);
extern int64_t reject              (uint32_t read_ptr,  uint32_t read_len,   int32_t error_code);
extern int64_t rollback            (uint32_t read_ptr,  uint32_t read_len,   int32_t error_code);
extern int64_t util_raddr          (uint32_t write_ptr, uint32_t write_len,
                                    uint32_t read_ptr,  uint32_t read_len);
extern int64_t util_accid          (uint32_t write_ptr, uint32_t write_len,
                                    uint32_t read_ptr,  uint32_t read_len);
extern int64_t util_verify         (uint32_t dread_ptr, uint32_t dread_len,
                                    uint32_t sread_ptr, uint32_t sread_len,
                                    uint32_t kread_ptr, uint32_t kread_len);
extern int64_t util_sha512h        (uint32_t write_ptr, uint32_t write_len,
                                    uint32_t read_ptr,  uint32_t read_len);
extern int64_t util_subfield       (uint32_t read_ptr,  uint32_t read_len, uint32_t field_id );                 
extern int64_t util_subarray       (uint32_t read_ptr,  uint32_t read_len, uint32_t array_id );                 
extern int64_t etxn_burden         (void );
extern int64_t etxn_details        (uint32_t write_ptr,  uint32_t write_len);
extern int64_t etxn_fee_base       (uint32_t tx_byte_count);
extern int64_t etxn_reserve        (uint32_t count);
extern int64_t etxn_generation     (void);
extern int64_t emit                (uint32_t read_ptr,   uint32_t read_len);
extern int64_t hook_account        (uint32_t write_ptr,  uint32_t write_len);
extern int64_t hook_hash           (uint32_t write_ptr,  uint32_t write_len);
extern int64_t fee_base            (void);
extern int64_t ledger_seq          (void);
extern int64_t nonce               (uint32_t write_ptr,  uint32_t write_len);
extern int64_t slot_clear          (uint32_t slot);
extern int64_t slot_set            (uint32_t read_ptr,   uint32_t read_len,
                                    uint32_t slot_type,  int32_t  slot);
extern int64_t slot_field_txt      (uint32_t write_ptr,  uint32_t write_len,
                                    uint32_t field_id,   uint32_t slot);
extern int64_t slot_field          (uint32_t write_ptr,  uint32_t write_len,
                                    uint32_t field_id,   uint32_t slot);
extern int64_t slot_id             (uint32_t slot);
extern int64_t slot_type           (uint32_t slot);
extern int64_t state_set           (uint32_t read_ptr,   uint32_t read_len,
                                    uint32_t kread_ptr,  uint32_t kread_len);
extern int64_t state               (uint32_t write_ptr,  uint32_t write_len,
                                    uint32_t kread_ptr,  uint32_t kread_len);
extern int64_t state_foreign       (uint32_t write_ptr,  uint32_t write_len,
                                    uint32_t kread_ptr,  uint32_t kread_len,
                                    uint32_t aread_ptr,  uint32_t aread_len);
extern int64_t trace_slot          (uint32_t slot);
extern int64_t trace               (uint32_t read_ptr,   uint32_t read_len,   uint32_t as_hex);
extern int64_t trace_num           (uint32_t read_ptr,   uint32_t read_len,   int64_t number);

extern int64_t otxn_burden         (void);
extern int64_t otxn_field_txt      (uint32_t write_ptr,  uint32_t write_len,  uint32_t field_id);
extern int64_t otxn_field          (uint32_t write_ptr,  uint32_t write_len,  uint32_t field_id);
extern int64_t otxn_generation     (void);
extern int64_t otxn_id             (uint32_t write_ptr,  uint32_t write_len);
extern int64_t otxn_type           (void);


#define SUCCESS  0                  // return codes > 0 are reserved for hook apis to return "success"
#define OUT_OF_BOUNDS  -1           // could not read or write to a pointer to provided by hook
#define INTERNAL_ERROR  -2          // eg directory is corrupt
#define TOO_BIG  -3                 // something you tried to store was too big
#define TOO_SMALL  -4               // something you tried to store or provide was too small
#define DOESNT_EXIST  -5            // something you requested wasn't found
#define NO_FREE_SLOTS  -6           // when trying to load an object there is a maximum of 255 slots
#define INVALID_ARGUMENT  -7        // self explanatory
#define ALREADY_SET  -8             // returned when a one-time parameter was already set by the hook
#define PREREQUISITE_NOT_MET  -9    // returned if a required param wasn't set before calling
#define FEE_TOO_LARGE  -10          // returned if the attempted operation would result in an absurd fee
#define EMISSION_FAILURE  -11       // returned if an emitted tx was not accepted by rippled
#define TOO_MANY_NONCES  -12        // a hook has a maximum of 256 nonces
#define TOO_MANY_EMITTED_TXN  -13   // a hook has emitted more than its stated number of emitted txn
#define NOT_IMPLEMENTED  -14        // an api was called that is reserved for a future version
#define INVALID_ACCOUNT  -15        // an api expected an account id but got something else
#define GUARD_VIOLATION  -16        // a guarded loop or function iterated over its maximum
#define INVALID_FIELD  -17          // the field requested is returning sfInvalid
#define PARSE_ERROR  -18            // hook asked hookapi to parse something the contents of which was invalid


#include "sfcodes.h"
#include "hookmacro.h"

#endif
