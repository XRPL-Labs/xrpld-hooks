#include <stdint.h>
#include "hookapi.h"

int64_t hook(int64_t reserved) __attribute__((used));
int64_t cbak(int64_t reserved) __attribute__((used));

int64_t cbak(int64_t reserved) 
{
    accept(0,0,0);
    return 0;
}

int64_t hook(int64_t reserved ) {

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
    if (account_field_len < 20)                                   // negative values indicate errors from every api
        rollback(SBUF("Liteacc: sfAccount field missing!!!"), 1); // this code could never be hit in prod
                                                                  // but it's here for completeness
                                                                  
    // compare the "From Account" (sfAccount) on the transaction with the account the hook is running on
    int equal = 0; BUFFER_EQUAL(equal, hook_accid, account_field, 20);
    if (equal)
        accept(SBUF("Liteacc: Outgoing transaction"), 2);

    // execution to here means the user has sent a valid transaction TO the account the hook is installed on
   

    /**
     * Lite account example has three modes of operation
     * ------------------------------------------------
     * 1. Setup:        At least 1 XRP is sent, with an invoice id set to a public key of a dummy account
     *                  If the lite account associated with that public key exists the 1 XRP is deposited
     *                  If the lite account associated with that public key does not exist it is created.
     *                  A receiving tag is assigned to that public key to provide the end user an X-addr.
     *
     * 2. Receiving:    Invoice ID may be set to the public key or dest tag may be used. A fee is taken
     *                  and the balance is adjusted.
     *
     * 3. Sending:      Any account may send a message to the hook relaying a send instruction on behalf
     *                  of the lite account.
     *                  The Invoice ID must be absent or 0.
     *                  The transaction must be for at least 0.1 XRP
     *                  The transaction must include a memo containing a valid signed transaction on behalf
     *                  of the lite-account's key pair.
     **/

    int mode = -1;

    uint8_t invoice_id[32]; int64_t invoice_id_len = otxn_field(SBUF(invoice_id), sfInvoiceID);
    TRACEVAR(invoice_id_len);   // TRACEVAR causes rippled to output this variable in its trace() log

    int64_t dest_tag = -1; OTXN_FIELD_AS_UINT32(dest_tag, sfDestinationTag);
    TRACEVAR(dest_tag);

    uint8_t memos[2048];
    int64_t memos_len = otxn_field(SBUF(memos), sfMemos);

    TRACEVAR(memos_len);
    if (memos_len > 0)
        trace(memos, memos_len, 1);

    // minimum memo length to be a valid memo would be 28 + 221 = 249
    if (memos_len < 249)
        memos_len = -1;

//    unsigned char valid_memo_type[] = { 0x7C, 0x06, 0x74, 0x78, 0x62, 0x6C, 0x6F, 0x62 };




    accept(0,0,0);
    return 0;
    /*
    if (invoice_id_len != 32 && dest_tag == -1)
    {
        // this is an invalid state, we don't know what to do 
        rollback(SBUF("NOT IMPLEMENTED YET"), 1000);

    }
*/
    // Execution to here means we are in mode 1 or 2

    // Check if the invoice_id (dummy public key) already exists in our state... but first check if 
    // the user supplied key is in the reserved state keys (so that a malicious actor can't clobber them).
    // Reserved state keys begin with 28 zeros and end with 4 bytes indicating a uin32_t destination tag.
    // Note that when a loop is written in a hook it must be "GUARD"ed. This tells rippled how many
    // iterations to expect in this loop, and the value must be a constant expr. If the loop iterates past
    // the limit set the hook immediately turns into a rollback(). This prevents infinite loops and resource
    // abuse.
    int all_zeros = 1;
    for (int i = 0; GUARD(28), all_zeros && i < 28; ++i)
        if (invoice_id[i] != 0)
            all_zeros = 0;

    if (all_zeros)
        rollback(SBUF("Liteacc: invalid public key supplied in invoice_id"), 3);

/*



    // fetch the sent Amount
    // Amounts can be 384 bits or 64 bits. If the Amount is an XRP value it will be 64 bits.
    unsigned char amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    int64_t drops_to_send = 1000; // this will be the default


    if (amount_len != 8)
    {
        // you can trace the behaviour of your hook using the trace(buf, size, as_hex) api
        // which will output to xrpld's trace log
        trace(SBUF("Carbon: Non-xrp transaction detected, sending default 1000 drops to rfCarbon"), 0);
    } else
    {
        trace(SBUF("Carbon: XRP transaction detected, computing 1% to send to rfCarbon"), 0);
        int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
        TRACEVAR(otxn_drops); 
        if (otxn_drops > 100000)   // if its less we send the default amount. or if there was an error we send default
            drops_to_send = (int64_t)((double)otxn_drops * 0.01f); // otherwise we send 1%
    }

    TRACEVAR(drops_to_send);    

    // hooks communicate accounts via the 20 byte account ID, this can be generated from an raddr like so
    // a more efficient way to do this is precompute the account-id from the raddr (if the raddr never changes)
    uint8_t carbon_accid[20];
    int64_t ret = util_accid( 
            SBUF(carbon_accid),                                 
            SBUF("rfCarbonVNTuXckX6x2qTMFmFSnm6dEWGX") );         
    TRACEVAR(ret);

    // fees for emitted transactions are based on how many txn your hook is emitted, whether or not this triggering
    // was caused by a previously emitted transaction and how large the new emitted transaction is in bytes
    // we need to precompute this before populating the payment transaction, as it is a field inside the tx
    int64_t fee_base = etxn_fee_base(PREPARE_PAYMENT_SIMPLE_SIZE);

    // create a buffer to write the emitted transaction into
    unsigned char tx[PREPARE_PAYMENT_SIMPLE_SIZE];

    // we will use an XRP payment macro, this will populate the buffer with a serialized binary transaction
    // Parameter list: ( buf_out, drops_amount, drops_fee, to_address, dest_tag, src_tag )
    PREPARE_PAYMENT_SIMPLE(tx, drops_to_send, fee_base, carbon_accid, 0, 0);

    // emit the transaction
    emit(SBUF(tx));

    // accept and allow the original transaction through
    accept(SBUF("Carbon: Emitted a transaction"), 0); 
    return 0;
*/
}
