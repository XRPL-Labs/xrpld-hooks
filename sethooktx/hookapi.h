#include <stdint.h>
#include "sfcodes.h"

extern int64_t accept                  ( int32_t error_code, unsigned char* buf_out, uint32_t buf_len );
extern int64_t emit_txn                ( unsigned char* buf_out, uint32_t buf_len );
extern int64_t get_burden              ( void );
extern int64_t get_emit_burden         ( void );
extern int64_t get_emit_fee_base       ( uint32_t tx_byte_count);
extern int64_t get_fee_base            ( void );
extern int64_t get_generation          ( void );
extern int64_t get_hook_account        ( unsigned char* buf_in );
extern int64_t get_ledger_seq          ( void );
extern int64_t get_nonce               ( unsigned char* buf_in );
extern int64_t get_obj_by_hash         ( unsigned char* buf_out );
extern int64_t get_pseudo_details      ( unsigned char* buf_in, uint32_t buf_len );
extern int64_t get_pseudo_details_size ( void );
extern int64_t get_state               ( unsigned char* key_buf_out, uint32_t data_buf_in, uint32_t buf_in_len );
extern int64_t get_txn_field           ( uint32_t field_id, uint32_t buf_in, uint32_t buf_len );
extern int64_t get_txn_id              ( void );
extern int64_t get_txn_type            ( void );
extern int64_t output_dbg              ( uint32_t buf_out, uint32_t buf_len );
extern int64_t output_dbg_obj          ( uint32_t slot );
extern int64_t reject                  ( int32_t error_code, uint32_t buf_out, uint32_t out_len );
extern int64_t rollback                ( int32_t error_code, uint32_t buf_out, uint32_t out_len );
extern int64_t set_emit_count          ( uint32_t c );
extern int64_t set_state               ( uint32_t key_buf_out, uint32_t data_buf_out, uint32_t buf_out_len );


#define DPRINT(x)\
    {output_dbg((x), sizeof((x)));}

#define BUF_TO_DEC(buf, len, numout)\
    {\
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

#define DEC_TO_BUF(numin, buf, len)\
    {\
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
    


// example simple payment
/*
 * 120000                                       // 3 tt = payment
 * 2280000000                                   // 5 flags = tfCanonical 
 * 2400000000                                   // 5 sequence = 2
 * 201B00000000                                 // 6 last ledger sequence
 * 614000000000000000                           // 9 amount of XRP = 1601.297166
 * 68400000000000000C                           // 9 amount of fee = 12 drops
 * 81140000000000000000000000000000000000000000 // 22 account from
 * 83140000000000000000000000000000000000000000 // 22 acount to
 */


#define PREPARE_PAYMENT_SIMPLE_SIZE 126 
#define PREPARE_PAYMENT_SIMPLE(buf_out_master, drops_amount, drops_fee, to_address, dest_tag, src_tag)\
    {\
        uint8_t* buf_out = buf_out_master;\
        uint8_t acc[20];\
        get_hook_account(acc);\
        _01_02_ENCODE_TT                   (buf_out, ttPAYMENT                      );      /* uint16  | size  3 */ \
        _02_02_ENCODE_FLAGS                (buf_out, tfCANONICAL                    );      /* uint32  | size  5 */ \
        _02_03_ENCODE_TAG_SRC              (buf_out, src_tag                        );      /* uint32  | size  5 */ \
        _02_04_ENCODE_SEQUENCE             (buf_out, 0                              );      /* uint32  | size  5 */ \
        _02_14_ENCODE_TAG_DST              (buf_out, dest_tag                       );      /* uint32  | size  5 */ \
        _02_27_ENCODE_LLS                  (buf_out, get_ledger_seq() + 5           );      /* uint32  | size  6 */ \
        _06_01_ENCODE_DROPS_AMOUNT         (buf_out, drops_amount                   );      /* amount  | size  9 */ \
        _06_08_ENCODE_DROPS_FEE            (buf_out, drops_fee                      );      /* amount  | size  9 */ \
        _07_03_ENCODE_SIGNING_PUBKEY_NULL  (buf_out                                 );      /* pk      | size 35 */ \
        _08_01_ENCODE_ACCOUNT_SRC          (buf_out, acc                            );      /* account | size 22 */ \
        _08_03_ENCODE_ACCOUNT_DST          (buf_out, to_address                     );      /* account | size 22 */ \
    }



