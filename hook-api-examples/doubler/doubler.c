#include <stdint.h>
#include "../hookapi.h"

int64_t cbak(int64_t reserved)
{
    accept(0,0,0);
    return 0;
}

int64_t hook(int64_t reserved ) {

    // before we start calling hook-api functions we should tell the hook how many tx we intend to create
    etxn_reserve(1); // we are going to emit 1 transaction

    // this api fetches the AccountID of the account the hook currently executing is installed on
    // since hooks can be triggered by both incoming and ougoing transactions this is important to know
    unsigned char hook_accid[20];
    hook_account((uint32_t)hook_accid, 20);

    // NB:
    //  almost all of the hook apis require a buffer pointer and buffer length to be supplied ... to make this a
    //  little easier to code a macro: `SBUF(your_buffer)` expands to `your_buffer, sizeof(your_buffer)`

    // next fetch the sfAccount field from the originating transaction
    uint8_t account_field[20];
    int32_t account_field_len = otxn_field(SBUF(account_field), sfAccount);
    TRACEVAR(account_field_len);

    // compare the "From Account" (sfAccount) on the transaction with the account the hook is running on
    int equal = 0; BUFFER_EQUAL(equal, hook_accid, account_field, 20);
    if (equal)
    {
        // if the accounts are equal (memcmp == 0) the otxn was originated by the hook account
        // accept() it and end the hook execution here
        accept(SBUF("Doubler: Incoming transaction"), 2);
        return 0;
    }

    // execution to here means the user has sent a valid transaction TO the account the hook is installed on

    // fetch the sent Amount
    // Amounts can be 384 bits or 64 bits. If the Amount is an XRP value it will be 64 bits.
    unsigned char amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    int64_t drops_to_send = AMOUNT_TO_DROPS(amount_buffer) * 2; // doubler pays back 2x received


    if (amount_len != 8)
    {
        // you can trace the behaviour of your hook using the trace(buf, size, as_hex) api
        // which will output to xrpld's trace log
        trace(SBUF("Doubler: Non-xrp transaction detected, accepting without doubling"), 0);
        accept(SBUF("Doubler: Incoming transaction"), 2);
        return 0;
    }

    TRACEVAR(drops_to_send);

    // fees for emitted transactions are based on how many txn your hook is emitted, whether or not this triggering
    // was caused by a previously emitted transaction and how large the new emitted transaction is in bytes
    // we need to precompute this before populating the payment transaction, as it is a field inside the tx
    int64_t fee_base = etxn_fee_base(PREPARE_PAYMENT_SIMPLE_SIZE);

    // create a buffer to write the emitted transaction into
    unsigned char tx[PREPARE_PAYMENT_SIMPLE_SIZE];

    // we will use an XRP payment macro, this will populate the buffer with a serialized binary transaction
    // Parameter list: ( buf_out, drops_amount, drops_fee, to_address, dest_tag, src_tag )
    PREPARE_PAYMENT_SIMPLE(tx, drops_to_send, fee_base, account_field, 0, 0);

    // emit the transaction
    emit(SBUF(tx));

    // accept and allow the original transaction through
    accept(SBUF("Doubler: Emitted a transaction"), 0);
    return 0;

}
