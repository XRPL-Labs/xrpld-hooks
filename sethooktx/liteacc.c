#include <stdint.h>
#include "hookapi.h"

/**
 * RH TODO
 *  - handle a callback
 *  - rollback a send (minus a fee) if callback doesnt trigger within X ledgers
 */

int64_t hook(int64_t reserved) __attribute__((used));
int64_t cbak(int64_t reserved) __attribute__((used));

int64_t cbak(int64_t reserved)
{
    accept(0,0,0);
    return 0;
}

#define NEW_ACCOUNT_FEE_DROPS 1000000
#define HOOK_USAGE_FEE_DROPS 100000
#define ADDITIONAL_SEND_FEE_DROPS 0
#define SUBSIDIZED_SENDING 1            /* if set to 1 then the hook acc pays outgoing fees */

int64_t hook(int64_t reserved )
{

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
        rollback(SBUF("Liteacc: sfAccount field missing!!!"), 10); // this code could never be hit in prod
                                                                  // but it's here for completeness

    // compare the "From Account" (sfAccount) on the transaction with the account the hook is running on
    int equal = 0; BUFFER_EQUAL(equal, hook_accid, account_field, 20);
    if (equal)
        accept(SBUF("Liteacc: Outgoing transaction"), 20);

    /**
     * Lite account example has the following modes of operation
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

    // check the amount of XRP sent with this transaction
    uint8_t amount_buffer[8];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    // if it's negative then it's a non-XRP amount, or alternatively if the MSB is set
    if (amount_len != 8 || amount_buffer[0] & 0x80) /* non-xrp amount */
        rollback(SBUF("Liteacc: Non-xrp balance sent. Can't proceed!"), 30);

    // this is the actual value of the amount of XRP sent to the hook in this transaction
    int64_t amount_sent = AMOUNT_TO_DROPS(amount_buffer);

    if (amount_sent < HOOK_USAGE_FEE_DROPS)
        rollback(SBUF("Liteacc: Insufficient drops sent to do anything!"), 40);

    // check for the presence of a memo
    uint8_t memos[2048];
    int64_t memos_len = otxn_field(SBUF(memos), sfMemos);
    TRACEVAR(memos_len);
    trace(memos, memos_len, 1);

    // process outgoing send requests encoded in tx memos, if any
    if (memos_len > 0)
    {
        unsigned char* payload_ptr = 0;
        uint32_t payload_len = 0;

        unsigned char* signature_ptr = 0;
        uint32_t signature_len = 0;

        unsigned char* publickey_ptr = 0;
        uint32_t publickey_len = 0;

        for (int i = 0; GUARD(4), i < 3; ++i)
        {
            int64_t memo_lookup = util_subarray(memos, memos_len, i);
            TRACEVAR(memo_lookup);

            if (memo_lookup < 0)
                break; // invalid or too few memos
            unsigned char*  memo_ptr = SUB_OFFSET(memo_lookup) + memos;
            uint32_t memo_len = SUB_LENGTH(memo_lookup);

            // memos are nested inside an actual memo object
            memo_lookup = util_subfield(memo_ptr, memo_len, sfMemo);
            memo_ptr = SUB_OFFSET(memo_lookup) + memos;
            memo_len = SUB_LENGTH(memo_lookup);

            trace(SBUF("MEMO:"), 0);
            trace(memo_ptr, memo_len, 1);

            uint32_t memo_offset = memo_ptr - memos;
            TRACEVAR(memo_offset);
            TRACEVAR(memo_len);

            int64_t data_lookup = util_subfield(memo_ptr, memo_len, sfMemoData);
            int64_t format_lookup = util_subfield(memo_ptr, memo_len, sfMemoFormat);
            int64_t type_lookup = util_subfield(memo_ptr, memo_len, sfMemoType);

            TRACEVAR(data_lookup);
            TRACEVAR(format_lookup);
            TRACEVAR(type_lookup);

            if (data_lookup < 0 || format_lookup < 0 || type_lookup < 0)
                break;

            unsigned char* data_ptr = SUB_OFFSET(data_lookup) + memo_ptr;
            uint32_t data_len = SUB_LENGTH(data_lookup);

            TRACEVAR(data_len);

            unsigned char* format_ptr = SUB_OFFSET(format_lookup) + memo_ptr;
            uint32_t format_len = SUB_LENGTH(format_lookup);
            
            int is_payload = 0, is_signature = 0, is_publickey = 0;
            BUFFER_EQUAL_STR_GUARD(is_payload, format_ptr, format_len,   "signed/payload+1", 3);
            BUFFER_EQUAL_STR_GUARD(is_signature, format_ptr, format_len, "signed/signature+1", 3);
            BUFFER_EQUAL_STR_GUARD(is_publickey, format_ptr, format_len, "signed/publickey+1", 3);

            if (is_payload)
            {
                payload_ptr = data_ptr;
                payload_len = data_len;
            } else if (is_signature)
            {
                signature_ptr = data_ptr;
                signature_len = data_len;
            } else if (is_publickey)
            {
                publickey_ptr = data_ptr;
                publickey_len = data_len;
            }
        }

        trace(SBUF("payload:"), 0);
        TRACEVAR(payload_len);
        trace(payload_ptr, payload_len, 1);
        trace(SBUF("signature:"), 0);
        trace(signature_ptr, signature_len, 1);
        TRACEVAR(signature_len);
        trace(SBUF("public_key:"), 0);
        TRACEVAR(publickey_len);
        trace(publickey_ptr, publickey_len, 1);


        if (!(payload_ptr && signature_ptr && publickey_ptr))
            rollback(SBUF("Liteacc: [3] Memo is an invalid format."), 50);

        if (!util_verify(payload_ptr,    payload_len,
                         signature_ptr,  signature_len,
                         publickey_ptr,  publickey_len))
            rollback(SBUF("Liteacc: [3] Invalid signature in memo."), 60);

        // execution to here means that BUFFER<payload_ptr,payload_len> contains a validly signed object
        // now check if it is properly constructed
        // the expected format is a generic STObject containing
        // - at least:      sfDestination       sfAmount[as XRP drops]       sfSequence     sfSourceTag sfPublicKey
        // - optionally:    sfDestinationTag
        // Sequence must be greater than the previously used Sequence (timestamp is desirable but not mandated)
        // Last sequence is encoded by state loop key = [0xFFFFFFFF ... <src tag>]

        int64_t lookup_seq      = util_subfield(payload_ptr, payload_len, sfSequence);
        int64_t lookup_stag     = util_subfield(payload_ptr, payload_len, sfSourceTag);
        int64_t lookup_dest     = util_subfield(payload_ptr, payload_len, sfDestination);
        int64_t lookup_dtag     = util_subfield(payload_ptr, payload_len, sfDestinationTag);
        int64_t lookup_amt      = util_subfield(payload_ptr, payload_len, sfAmount);
        int64_t lookup_pubkey   = util_subfield(payload_ptr, payload_len, sfPublicKey);


        TRACEVAR(lookup_seq);
        TRACEVAR(lookup_stag);
        TRACEVAR(lookup_dest);
        TRACEVAR(lookup_dtag);
        TRACEVAR(lookup_amt);
        TRACEVAR(lookup_pubkey);

        if (lookup_seq < 0 || lookup_stag < 0 || lookup_dest < 0 || lookup_amt < 0 || lookup_pubkey < 0)
            rollback(SBUF("Liteacc: [3] Validly signed memo lacked required STObject fields."), 70);

        uint32_t src_tag = UINT32_FROM_BUF(SUB_OFFSET(lookup_stag) + memos);
        uint32_t seq = UINT32_FROM_BUF(SUB_OFFSET(lookup_seq) + memos);
        uint64_t drops_to_send = AMOUNT_TO_DROPS(SUB_OFFSET(lookup_amt) + memos);
        uint8_t* destination = SUB_OFFSET(lookup_dest) + memos;
        uint32_t dest_tag = UINT32_FROM_BUF(SUB_OFFSET(lookup_dtag) + memos);

        if (drops_to_send <= 0)
            rollback(SBUF("Liteacc: [3] Invalid amount specified in STObject."), 80);

        uint8_t* pubkey_from_memo = SUB_OFFSET(lookup_pubkey);
        if (SUB_LENGTH(lookup_pubkey) != 33)
            rollback(SBUF("Liteacc: [3] Invalid public key provided in memo STObject."), 90);

        uint8_t state_request[32];
        CLEARBUF(state_request); // set every byte to 0
        UINT32_TO_BUF(state_request + 28,  src_tag);

        // first verify if the src tag they have stated is their userid is actually connected to their pub key
        uint8_t pubkey_from_state[32];
        if (state(SBUF(pubkey_from_state), SBUF(state_request)) != 32)
            rollback(SBUF("Liteacc: [3] No lite account was associated with the supplied source tag."), 100);

        int is_equal = 0;
        BUFFER_EQUAL(is_equal, pubkey_from_state, pubkey_from_memo + 1 /* skip leading byte */, 32);
        if (!is_equal)
            rollback(SBUF("Liteacc: [3] Src tag did not match public key on file for this account."), 110);

        // change first 4 bytes to all 1 bits so we can lookup sequence number
        UINT32_TO_BUF(state_request, 0xFFFFFFFFUL);
        uint8_t last_seq_buf[4];
        if (state(SBUF(last_seq_buf), SBUF(state_request)) != 4)
            rollback(SBUF("Liteacc: [3] Last sequence not found in lookup."), 120);

        uint32_t last_seq = UINT32_FROM_BUF(last_seq_buf);
        if (last_seq >= seq)
            rollback(SBUF("Liteacc: [3] Last sequence is >= provided sequence number."), 130);

        // lookup user's balance
        uint8_t balance_buf[8];
        if (state(SBUF(balance_buf), SBUF(pubkey_from_state)) != 8)
            rollback(SBUF("Liteacc: [3] Could not retrieve user's balance."), 140);

        // prepare to emit a transaction
        etxn_reserve(1);
        int64_t fee_base = etxn_fee_base(PREPARE_PAYMENT_SIMPLE_SIZE);

        uint64_t billable = drops_to_send + ADDITIONAL_SEND_FEE_DROPS + HOOK_USAGE_FEE_DROPS + 
            ( SUBSIDIZED_SENDING ? 0 : fee_base );

        if (billable <= 0 || billable < drops_to_send)
            rollback(SBUF("Liteacc: [3] Invariant tripped."), 145);

        uint64_t balance = UINT64_FROM_BUF(balance_buf);
        if (balance < billable)
        {
            RBUF2(out, out_len, "Liteacc: [3] User balance ", balance, " less than required ", billable);
            rollback(out, out_len, 150);
        }

        uint64_t new_balance = balance - billable;

        // defensive sanity check
        if (balance <= new_balance)
            rollback(SBUF("Liteacc: [3] Invariant tripped."), 160);

        CLEARBUF(balance_buf);

        // write balance
        UINT64_TO_BUF(balance_buf, balance);
        if (state_set(SBUF(balance_buf), SBUF(pubkey_from_state)) != 8)
            rollback(SBUF("Liteacc: [3] Failed to set new balance for user."), 170);

        // create a buffer to write the emitted transaction into
        unsigned char tx[PREPARE_PAYMENT_SIMPLE_SIZE];

        // we will use an XRP payment macro, this will populate the buffer with a serialized binary transaction
        // Parameter list: ( buf_out, drops_amount, drops_fee, to_address, dest_tag, src_tag )
        PREPARE_PAYMENT_SIMPLE(tx, drops_to_send, fee_base, destination, dest_tag, src_tag);

        // emit the transaction
        emit(SBUF(tx));

        // accept
        RBUF2(out, out_len, "Liteacc: [3] Successfully emitted ", drops_to_send, ", new balance: ", new_balance);
        accept(out, out_len, 0);
        // execution will not occur past here
    }


    // the operation of this HOOK is reasonably complex so for demonstration purposes we collect
    // as much data about the situtation as possible and push execution logic to the end
    int32_t have_pubkey = 0, new_user = 1;
    uint8_t user_pubkey[32];

    // check for the presence of an invoice id
    int64_t invoice_id_len = otxn_field(SBUF(user_pubkey), sfInvoiceID);

    // ensure the invoice id can be used as a lookup key (if it has been provided)
    if (invoice_id_len == 32)
    {
        // because we also store destination tags in our state key-map we don't want to allow maliciously
        // crafted invoice_ids to cause the hook to retrieve the wrong value type from the map
        // further, we use key=[0xFFFFFFFF... <src tag>] for storing account sequence
        // so also ensure they have not provided this in invoice id
        int found_a_zero = 0;
        int front_count = 0;    // count the bytes that are 'all 1 bits' at the front of the key
        int back_count = 0;     // count the bytes that are 'all 0 bits' at the back of the key
        // note here we use a loop but in order to do that we need to guard the loop or it will be rejected
        for (int i = 0; GUARD(28), i < 28; ++i)
        {
            if (user_pubkey[i] != 0xFFU)
                found_a_zero = 1;

            if (user_pubkey[i] == 0xFFU && !found_a_zero)
                front_count++;

            if (user_pubkey[i] == 0x00U && found_a_zero)
                back_count++;
        }

        if (back_count == 28 || front_count == 4 && back_count == 24)
            rollback(SBUF("Liteacc: [0] invalid public key supplied in invoice_id"), 180);

        have_pubkey = 1;
    }


    // check for the presence of a destination tag
    // with most APIs we can feed in 0,0 to return a small field as an int64_t
    int64_t dest_tag_raw = otxn_field(0, 0, sfDestinationTag);
    // destintation tags are stored as 4 byte unsigned integers but the above routine returns them as 64bit,
    // so do a safe cast here... a negative value indicates the lookup failed which is also converted here to dt=0
    uint32_t dest_tag = (dest_tag_raw > 0 ? (uint32_t)(dest_tag_raw & 0xFFFFFFFFULL) : 0);

    // a user account can be credited via a destination tag but we need to look up their public key
    if (dest_tag > 0 && !have_pubkey)
    {
        // construct our dtag state lookup key (must be 256 bits)
        uint8_t keybuf[32];
        CLEARBUF(keybuf); // zero pad the left
        UINT32_TO_BUF(keybuf + 28, dest_tag); // write the dest tag into the last four bytes

        if (state(SBUF(user_pubkey), SBUF(keybuf)) > 0) // perform the state lookup
            have_pubkey = 1; // if it is successful we now have a public key for this user from the dtag
        else
            rollback(SBUF("Liteacc: [2] dest tag suppled with transaction was not connected to a liteacc."), 190);
    }

    // by this point in the hook we should definitely have a public key for the lite account, either because
    // the user supplied it in the invoice id or because the user supplied a destination tag which we looked up
    if (!have_pubkey)
        rollback(
            SBUF("Liteacc: [1/2] Cannot continue without a lite acc public key, please supply via InvoiceID"), 200);

    // fetch the user's balance if they have one (if they don't they are a new user)
    uint64_t user_balance = 0;
    uint8_t  user_balance_buffer[8];
    int64_t  user_balance_len = 0;
    user_balance_len = state(SBUF(user_balance_buffer), SBUF(user_pubkey));
    user_balance = UINT64_FROM_BUF(user_balance_buffer);
    if (user_balance_len > 0)
        new_user = 0;
    else
        user_balance = 0;



    TRACEVAR(user_balance);
    // mode 2 receiving or mode 1 setup
    if (new_user && amount_sent < NEW_ACCOUNT_FEE_DROPS)
        rollback(SBUF("Liteacc: [1] Insufficient drops sent to create a new account."), 210);

    if (!have_pubkey)
        rollback(SBUF("Liteacc: [1|2] Must provide public key in InvoiceID field or user-id in DestTag."), 220);

    if (new_user)
        user_balance = amount_sent - NEW_ACCOUNT_FEE_DROPS;
    else
        user_balance += (amount_sent - HOOK_USAGE_FEE_DROPS);

    if (user_balance < 0)
        rollback(SBUF("Liteacc: [1|2] Invariant tripped; user_balance less than 0."), 230);

    // encode the modified balance
    UINT64_TO_BUF(user_balance_buffer, user_balance);

    TRACEVAR(user_balance);

    int64_t state_set_result = state_set(SBUF(user_balance_buffer), SBUF(user_pubkey));
    TRACEVAR(state_set_result);
    if (state_set_result < 0)
        rollback(SBUF("Liteacc: [1|2] Failed to create or update user account."), 240);

    if (new_user)
    {
        // assign a destination tag to this user
        // first get the destination tag we are up to (this is all 0's key)
        uint8_t  dtag_counter_buffer[4];
        uint8_t  dtag_counter_key[32];
        CLEARBUF(dtag_counter_key);
        int64_t  dtag_counter_len = state(SBUF(dtag_counter_buffer), SBUF(dtag_counter_key));

        if (dtag_counter_len < 0) // first time the hook has been run!
            dest_tag = 1;
        else
            dest_tag = UINT32_FROM_BUF(dtag_counter_buffer) + 1;

        TRACEVAR(dest_tag);

        UINT32_TO_BUF(dtag_counter_buffer, dest_tag);

        // update the counter
        if (state_set(SBUF(dtag_counter_buffer), SBUF(dtag_counter_key)) < 0)
            rollback(SBUF("Liteacc: [1] Could not assign new user a destination tag."), 250);

        // create a pointer from the dest tag to the user's public key in case we receive a payment
        // to this destination tag (so we can find out which user it is for)
        uint8_t user_dtag_key[32];
        CLEARBUF(user_dtag_key); // the first 28 bytes of the key are 0
        UINT32_TO_BUF(user_dtag_key+28, dest_tag); // the last 4 bytes of the key are the dtag

        if (state_set(SBUF(user_pubkey), SBUF(user_dtag_key)) < 0)
            rollback(SBUF("Liteacc: [1] Could not assign new user a destination tag."), 260);

        RBUF2(out, out_len, "Liteacc: [1] New user's balance is ", user_balance, " and dest tag is ", dest_tag);
        accept(out, out_len, 0);
    }

    // format a string to return in meta
    RBUF(out, out_len, "Liteacc: [2] User balance is ", user_balance);
    // return the string and accept the transaction
    accept(out, out_len, 0);
    return 0;
}
