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

extern int32_t _g                  (uint32_t id, uint32_t maxiter);
extern int64_t accept              (uint32_t read_ptr,   uint32_t read_len,   int32_t error_code);
extern int64_t reject              (uint32_t read_ptr,   uint32_t read_len,   int32_t error_code);
extern int64_t rollback            (uint32_t read_ptr,   uint32_t read_len,   int32_t error_code);
extern int64_t util_raddr          (uint32_t write_ptr,    uint32_t write_len,
                                    uint32_t read_ptr,   uint32_t read_len);
extern int64_t util_accid          (uint32_t write_ptr,    uint32_t write_len,
                                    uint32_t read_ptr,   uint32_t read_len);
extern int64_t util_verify         (uint32_t sread_ptr,  uint32_t sread_len,
                                    uint32_t kread_ptr,  uint32_t kread_len);
extern int64_t util_sha512h        (uint32_t write_ptr,    uint32_t write_len,
                                    uint32_t read_ptr,   uint32_t read_len);
extern int64_t etxn_burden         (void );
extern int64_t etxn_details        (uint32_t write_ptr,    uint32_t write_len);
extern int64_t etxn_fee_base       (uint32_t tx_byte_count);
extern int64_t etxn_reserve        (uint32_t count);
extern int64_t etxn_generation     (void);
extern int64_t emit                (uint32_t read_ptr,   uint32_t read_len);
extern int64_t hook_account        (uint32_t write_ptr,    uint32_t write_len);
extern int64_t hook_hash           (uint32_t write_ptr,    uint32_t write_len);
extern int64_t fee_base            (void);
extern int64_t ledger_seq          (void);
extern int64_t nonce               (uint32_t write_ptr,    uint32_t write_len);
extern int64_t slot_clear          (uint32_t slot);
extern int64_t slot_set            (uint32_t read_ptr,   uint32_t read_len,
                                    uint32_t slot_type, int32_t  slot);
extern int64_t slot_field_txt      (uint32_t write_ptr,    uint32_t write_len,
                                    uint32_t field_id,  uint32_t slot);
extern int64_t slot_field          (uint32_t write_ptr,    uint32_t write_len,
                                    uint32_t field_id,  uint32_t slot);
extern int64_t slot_id             (uint32_t slot);
extern int64_t slot_type           (uint32_t slot);
extern int64_t state_set           (uint32_t read_ptr,   uint32_t read_len,
                                    uint32_t kread_ptr,  uint32_t kread_len);
extern int64_t state               (uint32_t write_ptr,    uint32_t write_len,
                                    uint32_t kread_ptr,  uint32_t kread_len);
extern int64_t state_foreign       (uint32_t write_ptr,    uint32_t write_len,
                                    uint32_t kread_ptr,  uint32_t kread_len,
                                    uint32_t aread_ptr,  uint32_t aread_len);
extern int64_t trace_slot          (uint32_t slot);
extern int64_t trace               (uint32_t write_ptr,    uint32_t write_len,    uint32_t as_hex);
extern int64_t otxn_burden         (void);
extern int64_t otxn_field_txt      (uint32_t write_ptr,    uint32_t write_len,    uint32_t field_id);
extern int64_t otxn_field          (uint32_t write_ptr,    uint32_t write_len,    uint32_t field_id);
extern int64_t otxn_generation     (void);
extern int64_t otxn_id             (uint32_t write_ptr,    uint32_t write_len);
extern int64_t otxn_type           (void);

#include <stdint.h>
#include "sfcodes.h"
#include "hookmacros.h"

#endif
