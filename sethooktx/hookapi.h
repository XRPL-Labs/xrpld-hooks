/**
 * Hook API include for Webassembly XRPLD Hooks
 *
 * Note to the reader:
 * This include defines two types of things: external functions and macros
 * No internal functions are declared because a non-inlining compiler may produce
 * undesirable output.
 */

#include <stdint.h>
#include "sfcodes.h"


extern int64_t accept              (uint32_t out_ptr,   uint32_t out_len,   int32_t error_code);                    
extern int64_t reject              (uint32_t out_ptr,   uint32_t out_len,   int32_t error_code);                    
extern int64_t rollback            (uint32_t out_ptr,   uint32_t out_len,   int32_t error_code);                    
                                                                                                                         
extern int64_t util_raddr          (uint32_t in_ptr,    uint32_t in_len,                                        
                                    uint32_t out_ptr,   uint32_t out_len);

extern int64_t util_accid          (uint32_t in_ptr,    uint32_t in_len,                                        
                                    uint32_t out_ptr,   uint32_t out_len);                                        

extern int64_t util_verify         (uint32_t sout_ptr,  uint32_t sout_len,                                        
                                    uint32_t kout_ptr,  uint32_t kout_len);                                      

extern int64_t util_sha512h        (uint32_t in_ptr,    uint32_t in_len,                                        
                                    uint32_t out_ptr,   uint32_t out_len);                                       

extern int64_t etxn_burden         (void );                                                                          
extern int64_t etxn_details        (uint32_t in_ptr,    uint32_t in_len);                                      
extern int64_t etxn_fee_base       (uint32_t tx_byte_count);                                                   
extern int64_t etxn_reserve        (uint32_t count);                                                          
extern int64_t etxn_generation     (void);                                                                          
extern int64_t emit                (uint32_t out_ptr,   uint32_t out_len);                                        
                                                                                                                         
extern int64_t hook_account        (uint32_t in_ptr,    uint32_t in_len);                                      
extern int64_t hook_hash           (uint32_t in_ptr,    uint32_t in_len);                                      
                                                                                                                         
extern int64_t fee_base            (void);                                                                          
extern int64_t ledger_seq          (void);                                                                          
extern int64_t nonce               (uint32_t in_ptr,    uint32_t in_len);                                      
                                                                                                                         
extern int64_t slot_clear          (uint32_t slot);                                                           
extern int64_t slot_set            (uint32_t out_ptr,   uint32_t out_len,                                          
                                    uint32_t slot_type, int32_t  slot);                                        
                                                                                                                         
extern int64_t slot_field_txt      (uint32_t in_ptr,    uint32_t in_len,                                        
                                    uint32_t field_id,  uint32_t slot);

extern int64_t slot_field          (uint32_t in_ptr,    uint32_t in_len,                                        
                                    uint32_t field_id,  uint32_t slot);                                        

extern int64_t slot_id             (uint32_t slot);                                                           
extern int64_t slot_type           (uint32_t slot);                                                           
                                                                                                                         
extern int64_t state_set           (uint32_t out_ptr,   uint32_t out_len,                                         
                                    uint32_t kout_ptr,  uint32_t kout_len);

extern int64_t state               (uint32_t in_ptr,    uint32_t in_len,                                        
                                    uint32_t kout_ptr,  uint32_t kout_len); 

extern int64_t state_foreign       (uint32_t in_ptr,    uint32_t in_len,                                        
                                    uint32_t kout_ptr,  uint32_t kout_len,                                        
                                    uint32_t aout_ptr,  uint32_t aout_len);                                      
                                                                                                                         
extern int64_t trace_slot          (uint32_t slot);                                                           
extern int64_t trace               (uint32_t in_ptr,    uint32_t in_len,    uint32_t as_hex);                     
                                                                                                                         
extern int64_t otxn_burden         (void);                                                                          
extern int64_t otxn_field_txt      (uint32_t in_ptr,    uint32_t in_len,    uint32_t field_id);                   
extern int64_t otxn_field          (uint32_t in_ptr,    uint32_t in_len,    uint32_t field_id);                   
extern int64_t otxn_generation     (void);                                                                          
extern int64_t otxn_id             (uint32_t in_ptr,    uint32_t in_len);                                      
extern int64_t otxn_type           (void);    


#define DPRINT(x)\
    {trace((x), sizeof((x)));}

#define BUF_TO_DEC(buf_in, len_in, numout)\
    {\
        unsigned char* buf = (buf_in);\
        int len = (len_in);\
        int i = 0;\
        numout = 0;\
        for (; i < len && (buf)[i] != 0; ++i) {\
            if ((buf)[i] < '0' || (buf)[i] > '9') {\
                numout = 0;\
                break;\
            }\
            numout *= 10;\
            numout += ((buf)[i]-'0');\
        }\
    }

#define DEC_TO_BUF(numin_raw, buf_in, len_in)\
    {\
        int numin = (numin_raw);\
        unsigned char* buf = (buf_in);\
        int len = (len_in);\
        int digit_count = 0;\
        int numin2 = numin;\
        int numin3 = numin;\
        for (; numin2 > 0; ++digit_count) numin2 /= 10;\
        if (digit_count < len - 1) {\
            (buf)[digit_count] = 0;\
            for (; digit_count > 0; --digit_count, numin3 /= 10)\
                (buf)[digit_count-1] = '0' + (numin3 % 10);\
        }\
    }

#define TO_HEX( buf_out_master, buf_in, buf_in_len )\
    {\
        unsigned char* buf_out = buf_out_master;\
        for (int i = 0; i < buf_in_len; ++i) {\
            int low  = (buf_in[i] & 0xFU);\
            int high = (buf_in[i] >> 4U) & 0xFU;\
            *buf_out++ =(high < 10 ? high + '0' : (high - 10) + 'A' );\
            *buf_out++ =(low  < 10 ? low  + '0' : (low  - 10) + 'A' );\
        }\
    }

#define STRLEN(buf, maxlen, lenout)\
    {\
        lenout = 0;\
        for (; (buf)[lenout] != 0 && lenout < maxlen; ++lenout);\
    }


#define ttPAYMENT 0
#define tfCANONICAL 0x80000000UL

#define atACCOUNT 1U
#define atOWNER 2U
#define atDESTINATION 3U
#define atISSUER 4U
#define atAUTHORIZE 5U
#define atUNAUTHORIZE 6U
#define atTARGET 7U
#define atREGULARKEY 8U
#define atPSEUDOCALLBACK 9U

#define amAMOUNT 1U
#define amBALANCE 2U
#define amLIMITAMOUNT 3U
#define amTAKERPAYS 4U
#define amTAKERGETS 5U
#define amLOWLIMIT 6U
#define amHIGHLIMIT 7U
#define amFEE 8U
#define amSENDMAX 9U
#define amDELIVERMIN 10U
#define amMINIMUMOFFER 16U
#define amRIPPLEESCROW 17U
#define amDELIVEREDAMOUNT 18U

/**
 * RH NOTE -- PAY ATTENTION
 *
 * ALL 'ENCODE' MACROS INCREMENT BUF_OUT
 * THIS IS TO MAKE CHAINING EASY 
 * BUF_OUT IS A SACRIFICIAL POINTER 
 *
 * 'ENCODE' MACROS WITH CONSTANTS HAVE
 * ALIASING TO ASSIST YOU WITH ORDER
 * _TYPECODE_FIELDCODE_ENCODE_MACRO
 * TO PRODUCE A SERIALIZED OBJECT
 * IN CANONICAL FORMAT YOU MUST ORDER
 * FIRST BY TYPE CODE THEN BY FIELD CODE
 *
 * ALL 'PREPARE' MACROS PRESERVE POINTERS
 *
 **/


// Encode drops to serialization format
// consumes 9 bytes
#define ENCODE_DROPS_SIZE 9
#define ENCODE_DROPS(buf_out, drops, amount_type ) \
    {\
        uint8_t uat = amount_type; \
        uint64_t udrops = drops; \
        buf_out[0] = 0x60U +(uat & 0x0FU ); \
        buf_out[1] = 0b01000000 + (( udrops >> 56 ) & 0b00111111 ); \
        buf_out[2] = (udrops >> 48) & 0xFFU; \
        buf_out[3] = (udrops >> 40) & 0xFFU; \
        buf_out[4] = (udrops >> 32) & 0xFFU; \
        buf_out[5] = (udrops >> 24) & 0xFFU; \
        buf_out[6] = (udrops >> 16) & 0xFFU; \
        buf_out[7] = (udrops >>  8) & 0xFFU; \
        buf_out[8] = (udrops >>  0) & 0xFFU; \
        buf_out += ENCODE_DROPS_SIZE; \
    }

#define _06_XX_ENCODE_DROPS(buf_out, drops, amount_type )\
    ENCODE_DROPS(buf_out, drops, amount_type );

#define ENCODE_DROPS_AMOUNT(buf_out, drops )\
    ENCODE_DROPS(buf_out, drops, amAMOUNT );
#define _06_01_ENCODE_DROPS_AMOUNT(buf_out, drops )\
    ENCODE_DROPS_AMOUNT(buf_out, drops );

#define ENCODE_DROPS_FEE(buf_out, drops )\
    ENCODE_DROPS(buf_out, drops, amFEE );
#define _06_08_ENCODE_DROPS_FEE(buf_out, drops )\
    ENCODE_DROPS_FEE(buf_out, drops );

#define ENCODE_TT_SIZE 3
#define ENCODE_TT(buf_out, tt )\
    {\
        uint8_t utt = tt;\
        buf_out[0] = 0x12U;\
        buf_out[1] =(utt >> 8 ) & 0xFFU;\
        buf_out[2] =(utt >> 0 ) & 0xFFU;\
        buf_out += ENCODE_TT_SIZE; \
    }
#define _01_02_ENCODE_TT(buf_out, tt)\
    ENCODE_TT(buf_out, tt);


#define ENCODE_ACCOUNT_SIZE 22
#define ENCODE_ACCOUNT(buf_out, account_id, account_type)\
    {\
        uint8_t uat = account_type;\
        buf_out[0] = 0x80U + uat;\
        buf_out[1] = 0x14U;\
        for (int i = 0; i < 20; ++i)\
            buf_out[i + 2] = account_id[i];\
        buf_out += ENCODE_ACCOUNT_SIZE;\
    }
#define _08_XX_ENCODE_ACCOUNT(buf_out, account_id, account_type)\
    ENCODE_ACCOUNT(buf_out, account_id, account_type);

#define ENCODE_ACCOUNT_SRC_SIZE 22
#define ENCODE_ACCOUNT_SRC(buf_out, account_id)\
    ENCODE_ACCOUNT(buf_out, account_id, atACCOUNT);
#define _08_01_ENCODE_ACCOUNT_SRC(buf_out, account_id)\
    ENCODE_ACCOUNT_SRC(buf_out, account_id);

#define ENCODE_ACCOUNT_DST_SIZE 22
#define ENCODE_ACCOUNT_DST(buf_out, account_id)\
    ENCODE_ACCOUNT(buf_out, account_id, atDESTINATION);
#define _08_03_ENCODE_ACCOUNT_DST(buf_out, account_id)\
    ENCODE_ACCOUNT_DST(buf_out, account_id);

#define ENCODE_UINT32_COMMON_SIZE 5U
#define ENCODE_UINT32_COMMON(buf_out, i, field)\
    {\
        uint32_t ui = i; \
        uint8_t uf = field; \
        buf_out[0] = 0x20U +(uf & 0x0FU); \
        buf_out[1] =(ui >> 24 ) & 0xFFU; \
        buf_out[2] =(ui >> 16 ) & 0xFFU; \
        buf_out[3] =(ui >>  8 ) & 0xFFU; \
        buf_out[4] =(ui >>  0 ) & 0xFFU; \
        buf_out += ENCODE_UINT32_COMMON_SIZE; \
    }
#define _02_XX_ENCODE_UINT32_COMMON(buf_out, i, field)\
    ENCODE_UINT32_COMMON(buf_out, i, field)\

#define ENCODE_UINT32_UNCOMMON_SIZE 6U
#define ENCODE_UINT32_UNCOMMON(buf_out, i, field)\
    {\
        uint32_t ui = i; \
        uint8_t uf = field; \
        buf_out[0] = 0x20U; \
        buf_out[1] = uf; \
        buf_out[2] =(ui >> 24 ) & 0xFFU; \
        buf_out[3] =(ui >> 16 ) & 0xFFU; \
        buf_out[4] =(ui >>  8 ) & 0xFFU; \
        buf_out[5] =(ui >>  0 ) & 0xFFU; \
        buf_out += ENCODE_UINT32_UNCOMMON_SIZE; \
    }
#define _02_XX_ENCODE_UINT32_UNCOMMON(buf_out, i, field)\
    ENCODE_UINT32_UNCOMMON(buf_out, i, field)\

#define ENCODE_LLS_SIZE 6U
#define ENCODE_LLS(buf_out, lls )\
    ENCODE_UINT32_UNCOMMON(buf_out, lls, 0x1B );
#define _02_27_ENCODE_LLS(buf_out, lls )\
    ENCODE_LLS(buf_out, lls );

#define ENCODE_TAG_SRC_SIZE 5
#define ENCODE_TAG_SRC(buf_out, tag )\
    ENCODE_UINT32_COMMON(buf_out, tag, 0x3U );
#define _02_03_ENCODE_TAG_SRC(buf_out, tag )\
    ENCODE_TAG_SRC(buf_out, tag );

#define ENCODE_TAG_DST_SIZE 5
#define ENCODE_TAG_DST(buf_out, tag )\
    ENCODE_UINT32_COMMON(buf_out, tag, 0xEU );
#define _02_14_ENCODE_TAG_DST(buf_out, tag )\
    ENCODE_TAG_DST(buf_out, tag );

#define ENCODE_SEQUENCE_SIZE 5
#define ENCODE_SEQUENCE(buf_out, sequence )\
    ENCODE_UINT32_COMMON(buf_out, sequence, 0x4U );
#define _02_04_ENCODE_SEQUENCE(buf_out, sequence )\
    ENCODE_SEQUENCE(buf_out, sequence );

#define ENCODE_FLAGS_SIZE 5
#define ENCODE_FLAGS(buf_out, tag )\
    ENCODE_UINT32_COMMON(buf_out, tag, 0x2U );
#define _02_02_ENCODE_FLAGS(buf_out, tag )\
    ENCODE_FLAGS(buf_out, tag );

#define ENCODE_SIGNING_PUBKEY_SIZE 35
#define ENCODE_SIGNING_PUBKEY(buf_out, pkey )\
    {\
        buf_out[0] = 0x73U;\
        buf_out[1] = 0x21U;\
        for (int i = 0; i < 33; ++i)\
            buf_out[i+2] = pkey[i];\
        buf_out += ENCODE_SIGNING_PUBKEY_SIZE;\
    }

#define _07_03_ENCODE_SIGNING_PUBKEY(buf_out, pkey )\
    ENCODE_SIGNING_PUBKEY(buf_out, pkey );

#define ENCODE_SIGNING_PUBKEY_NULL_SIZE 35
#define ENCODE_SIGNING_PUBKEY_NULL(buf_out )\
    {\
        buf_out[0] = 0x73U;\
        buf_out[1] = 0x21U;\
        for (int i = 0; i < 33; ++i)\
            buf_out[i+2] = 0U;\
        buf_out += ENCODE_SIGNING_PUBKEY_NULL_SIZE;\
    }

#define _07_03_ENCODE_SIGNING_PUBKEY_NULL(buf_out )\
    ENCODE_SIGNING_PUBKEY_NULL(buf_out );
    

#define PREPARE_PAYMENT_SIMPLE_SIZE 231 
#define PREPARE_PAYMENT_SIMPLE(buf_out_master, drops_amount_raw, drops_fee_raw, to_address, dest_tag_raw, src_tag_raw)\
    {\
        uint8_t* buf_out = buf_out_master;\
        uint8_t acc[20];\
        uint64_t drops_amount = (drops_amount_raw);\
        uint64_t drops_fee = (drops_fee_raw);\
        uint32_t dest_tag = (dest_tag_raw);\
        uint32_t src_tag = (src_tag_raw);\
        hook_account(acc);\
        _01_02_ENCODE_TT                   (buf_out, ttPAYMENT                      );      /* uint16  | size   3 */ \
        _02_02_ENCODE_FLAGS                (buf_out, tfCANONICAL                    );      /* uint32  | size   5 */ \
        _02_03_ENCODE_TAG_SRC              (buf_out, src_tag                        );      /* uint32  | size   5 */ \
        _02_04_ENCODE_SEQUENCE             (buf_out, 0                              );      /* uint32  | size   5 */ \
        _02_14_ENCODE_TAG_DST              (buf_out, dest_tag                       );      /* uint32  | size   5 */ \
        _02_27_ENCODE_LLS                  (buf_out, ledger_seq() + 5           );      /* uint32  | size   6 */ \
        _06_01_ENCODE_DROPS_AMOUNT         (buf_out, drops_amount                   );      /* amount  | size   9 */ \
        _06_08_ENCODE_DROPS_FEE            (buf_out, drops_fee                      );      /* amount  | size   9 */ \
        _07_03_ENCODE_SIGNING_PUBKEY_NULL  (buf_out                                 );      /* pk      | size  35 */ \
        _08_01_ENCODE_ACCOUNT_SRC          (buf_out, acc                            );      /* account | size  22 */ \
        _08_03_ENCODE_ACCOUNT_DST          (buf_out, to_address                     );      /* account | size  22 */ \
        emit_details(buf_out, 105);                                                     /* emitdet | size 105 */ \
    }



