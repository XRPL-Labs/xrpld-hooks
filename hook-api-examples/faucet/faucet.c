#include <stdint.h>
#include "../hookapi.h"

#define DROPS_AMOUNT_PER_DISTRIBUTION (100000000000ULL) // 100,000 xrp

int64_t cbak(int64_t reserved)
{
    accept(0,0,0);
    return 0;
}

#define INVALID_MEMO(n)\
{\
    rollback(\
        SBUF("Faucet: Invalid Memo. Expect: {MemoData: raddr, MemoFormat: unsigned/payload+1, MemoType: *}"), n);\
    return 0;\
}
int64_t hook(int64_t reserved ) {

    // before we start calling hook-api functions we should tell the hook how many tx we intend to create
    etxn_reserve(1); // we are going to emit 1 transaction

    // this api fetches the AccountID of the account the hook currently executing is installed on
    // since hooks can be triggered by both incoming and ougoing transactions this is important to know
    unsigned char hook_accid[20];
    hook_account((uint32_t)hook_accid, 20);

    // next fetch the sfAccount field from the originating transaction
    {
        uint8_t account_field[20];
        int32_t account_field_len = otxn_field(SBUF(account_field), sfAccount);
        TRACEVAR(account_field_len);

        // compare the "From Account" (sfAccount) on the transaction with the account the hook is running on
        int equal = 0; BUFFER_EQUAL(equal, hook_accid, account_field, 20);
        if (equal)
        {
            accept(SBUF("Faucet: Outgoing transaction, skipping"), 1);
            return 0;
        }
    }
    // execution to here means the user has sent a valid transaction TO the account the hook is installed on

    // check for the presence of a memo
    uint8_t memos[2048];
    int64_t memos_len = otxn_field(SBUF(memos), sfMemos);

    if (memos_len <= 0)
        INVALID_MEMO(2);

    /* Memo: { MemoData: <app data>,   MemoFormat: "unsigned/payload+1",   MemoType: [application defined] } */

    // the memos are presented in an array object, which we must index into
    int64_t memo_lookup = util_subarray(memos, memos_len, 0);
    TRACEVAR(memo_lookup);

    if (memo_lookup < 0)
    {
        accept(SBUF("Faucet: Note: No memo attached, accepting regular incoming otxn."), 3);
        return 3;
    }

    // if the subfield/array lookup is successful we must extract the two pieces of returned data
    // which are, respectively, the offset at which the field occurs and the field's length
    uint8_t*  memo_ptr = SUB_OFFSET(memo_lookup) + memos;
    uint32_t  memo_len = SUB_LENGTH(memo_lookup);

    trace(SBUF("MEMO:"), 0);
    trace(memo_ptr, memo_len, 1);

    // memos are nested inside an actual memo object, so we need to subfield
    // equivalently in JSON this would look like memo_array[i]["Memo"]
    memo_lookup = util_subfield(memo_ptr, memo_len, sfMemo);
    memo_ptr = SUB_OFFSET(memo_lookup) + memo_ptr;
    memo_len = SUB_LENGTH(memo_lookup);

    // now we lookup the subfields of the memo itself
    // again, equivalently this would look like memo_array[i]["Memo"]["MemoData"], ... etc.
    int64_t data_lookup = util_subfield(memo_ptr, memo_len, sfMemoData);
    int64_t format_lookup = util_subfield(memo_ptr, memo_len, sfMemoFormat);

    // if any of these lookups fail the request is malformed
    if (data_lookup < 0 || format_lookup < 0)
        INVALID_MEMO(4);

    uint8_t* format_ptr = SUB_OFFSET(format_lookup) + memo_ptr;
    uint32_t format_len = SUB_LENGTH(format_lookup);
    
    uint8_t* data_ptr = SUB_OFFSET(data_lookup) + memo_ptr;
    uint32_t data_len = SUB_LENGTH(data_lookup);

    int equal = 0; BUFFER_EQUAL_STR_GUARD(equal, format_ptr, format_len, "unsigned/payload+1", 1);

    uint8_t account_field[20];
    if (util_accid(SBUF(account_field), data_ptr, data_len) != 20)
    {
        rollback(SBUF("Faucet: Invalid rAddress specified in incoming memo."), 5);
        return 5;
    }
     
    // check if this address has already been paid out
    uint8_t state_response[1];
    if (state(SBUF(state_response), SBUF(account_field)) == 1)
    {
        rollback(SBUF("Faucet: Provided address has already received a payment from this faucet."), 6);
        return 6;
    }

    // mark this address as paid out
    state_response[0] = 1;
    if (state_set(SBUF(state_response), SBUF(account_field)) != 1)
    {
        rollback(SBUF("Faucet: Unable to mark address as sent, bailing."), 7);
        return 7;
    }

    // we need to precompute this before populating the payment transaction, as it is a field inside the tx
    int64_t fee_base = etxn_fee_base(PREPARE_PAYMENT_SIMPLE_SIZE);

    // create a buffer to write the emitted transaction into
    unsigned char tx[PREPARE_PAYMENT_SIMPLE_SIZE];

    // we will use an XRP payment macro, this will populate the buffer with a serialized binary transaction
    // Parameter list: ( buf_out, drops_amount, drops_fee, to_address, dest_tag, src_tag )
    PREPARE_PAYMENT_SIMPLE(tx, DROPS_AMOUNT_PER_DISTRIBUTION, fee_base, account_field, 0, 0);

    // emit the transaction
    emit(SBUF(tx));

    // accept and allow the original transaction through
    accept(SBUF("Faucet: Emitted a transaction"), 0);
    return 0;
}
