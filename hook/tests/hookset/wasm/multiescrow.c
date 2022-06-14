// escrows are made to destination: rfCarbonVNTuXckX6x2qTMFmFSnm6dEWGX

#include "../../../examples/hookapi.h"

#define ttESCROW_CREATE 1U

/*
{
    8114 <accid>    "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
    120001          "TransactionType": "EscrowCreate",
    61 amt "Amount": "10000",

    "Destination": "rsA2LpzuawewSBQXkiju3YQTMzW13pAAdW",
    2024 "CancelAfter": 533257958,
    2025 "FinishAfter": 533171558,
    "Condition": "A0258020E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855810100",
    "DestinationTag": 23480,
    "SourceTag": 11747
}
*/

#define ENCODE_CANAFTER_SIZE 6U
#define ENCODE_CANAFTER(buf_out, caf )\
    ENCODE_UINT32_UNCOMMON(buf_out, caf, 36U );
#define _02_36_ENCODE_CANAFTER(buf_out, caf )\
    ENCODE_CANAFTER(buf_out, caf );

#define ENCODE_FINAFTER_SIZE 6U
#define ENCODE_FINAFTER(buf_out, faf )\
    ENCODE_UINT32_UNCOMMON(buf_out, faf, 37U );
#define _02_37_ENCODE_FINAFTER(buf_out, faf )\
    ENCODE_FINAFTER(buf_out, faf );

#ifdef HAS_CALLBACK
#define PREPARE_ESCROW_SIMPLE_SIZE 282U
#else
#define PREPARE_ESCROW_SIMPLE_SIZE 260U
#endif

#define PREPARE_ESCROW_SIMPLE(\
        buf_out_master, drops_amount_raw, to_address, dest_tag_raw, src_tag_raw, finish_after, cancel_after)\
{\
    uint8_t* buf_out = buf_out_master;\
    uint8_t acc[20];\
    uint64_t drops_amount = (drops_amount_raw);\
    uint32_t dest_tag = (dest_tag_raw);\
    uint32_t src_tag = (src_tag_raw);\
    uint32_t cls = (uint32_t)ledger_seq();\
    int64_t llt = ledger_last_time();\
    hook_account(SBUF(acc));\
    _01_02_ENCODE_TT                   (buf_out, ttESCROW_CREATE                );      /* uint16  | size   3 */ \
    _02_02_ENCODE_FLAGS                (buf_out, tfCANONICAL                    );      /* uint32  | size   5 */ \
    _02_03_ENCODE_TAG_SRC              (buf_out, src_tag                        );      /* uint32  | size   5 */ \
    _02_04_ENCODE_SEQUENCE             (buf_out, 0                              );      /* uint32  | size   5 */ \
    _02_14_ENCODE_TAG_DST              (buf_out, dest_tag                       );      /* uint32  | size   5 */ \
    _02_26_ENCODE_FLS                  (buf_out, cls + 1                        );      /* uint32  | size   6 */ \
    _02_27_ENCODE_LLS                  (buf_out, cls + 5                        );      /* uint32  | size   6 */ \
    _02_36_ENCODE_CANAFTER             (buf_out, llt + cancel_after             );      /* uint32  | size   6 */ \
    _02_37_ENCODE_FINAFTER             (buf_out, llt + finish_after             );      /* uint32  | size   6 */ \
    _06_01_ENCODE_DROPS_AMOUNT         (buf_out, drops_amount                   );      /* amount  | size   9 */ \
    uint8_t* fee_ptr = buf_out;\
    _06_08_ENCODE_DROPS_FEE            (buf_out, 0                              );      /* amount  | size   9 */ \
    _07_03_ENCODE_SIGNING_PUBKEY_NULL  (buf_out                                 );      /* pk      | size  35 */ \
    _08_01_ENCODE_ACCOUNT_SRC          (buf_out, acc                            );      /* account | size  22 */ \
    _08_03_ENCODE_ACCOUNT_DST          (buf_out, to_address                     );      /* account | size  22 */ \
    int64_t edlen = etxn_details((uint32_t)buf_out, PREPARE_ESCROW_SIMPLE_SIZE);       /* emitdet | size 1?? */ \
    int64_t fee = etxn_fee_base(buf_out_master, PREPARE_ESCROW_SIMPLE_SIZE);                                    \
    _06_08_ENCODE_DROPS_FEE            (fee_ptr, fee                            );                               \
}

int64_t hook(uint32_t reserved)
{
    etxn_reserve(2);

    uint8_t carbon_accid[20];
    int64_t ret = util_accid(
            SBUF(carbon_accid),                                   /* <-- generate into this buffer  */
            SBUF("rfCarbonVNTuXckX6x2qTMFmFSnm6dEWGX") );         /* <-- from this r-addr           */
    TRACEVAR(ret);


    uint8_t txhash[32];
    uint8_t txout[PREPARE_ESCROW_SIMPLE_SIZE];

    PREPARE_ESCROW_SIMPLE(txout, 1, carbon_accid, 0, 0, 1000, 2000);
    emit(SBUF(txhash), SBUF(txout));

    PREPARE_ESCROW_SIMPLE(txout, 1, carbon_accid, 0, 0, 1000, 2000);
    emit(SBUF(txhash), SBUF(txout));
    
    accept(0,0,0);

    _g(1,1);
    return 0;
}
