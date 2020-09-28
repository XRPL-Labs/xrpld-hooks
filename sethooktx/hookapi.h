
extern int64_t output_dbg   ( unsigned char* buf, int32_t len );
extern int64_t set_state    ( unsigned char* key_ptr, unsigned char* data_ptr_in, uint32_t in_len );
extern int64_t get_state    ( unsigned char* key_ptr, unsigned char* data_ptr_out, uint32_t out_len );
extern int64_t accept       ( int32_t error_code, unsigned char* data_ptr_in, uint32_t in_len );
extern int64_t reject       ( int32_t error_code, unsigned char* data_ptr_in, uint32_t in_len );
extern int64_t rollback     ( int32_t error_code, unsigned char* data_ptr_in, uint32_t in_len );
extern int64_t get_tx_type  ( );
extern int64_t get_tx_field     ( uint32_t field_id, uint32_t data_ptr_out, uint32_t out_len );
extern int64_t get_obj_by_hash  ( unsigned char* hash );
extern int64_t output_dbg_obj   ( int32_t slot );

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

#define STRLEN(buf, maxlen, lenout)\
    {\
        lenout = 0;\
        for (; (buf)[lenout] != 0 && lenout < maxlen; ++lenout);\
    }

#include "sfcodes.h"

