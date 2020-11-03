#include <stdint.h>
#include "hookapi.h"

int64_t hook(int64_t reserved) __attribute__((used));
int64_t cbak(int64_t reserved) __attribute__((used));

int64_t cbak(int64_t reserved) 
{
    accept(0,0,0);
    return 0;
}

#define NEW_ACCOUNT_FEE_DROPS 1000000
#define HOOK_USAGE_FEE_DROPS 100000

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
        rollback(SBUF("Liteacc: sfAccount field missing!!!"), 1); // this code could never be hit in prod
                                                                  // but it's here for completeness
                                                                  
    // compare the "From Account" (sfAccount) on the transaction with the account the hook is running on
    int equal = 0; BUFFER_EQUAL(equal, hook_accid, account_field, 20);
    if (equal)
        accept(SBUF("Liteacc: Outgoing transaction"), 2);
   
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
        rollback(SBUF("Liteacc: Non-xrp balance sent. Can't proceed!"), 1);

    // this is the actual value of the amount of XRP sent to the hook in this transaction
    int64_t amount_sent = AMOUNT_TO_DROPS(amount_buffer);

    if (amount_sent < HOOK_USAGE_FEE_DROPS)
        rollback(SBUF("Liteacc: Insufficient drops sent to do anything!"), 2);

    // the operation of this HOOK is reasonably complex so for demonstration purposes we collect
    // as much data about the situtation as possible and push execution logic to the end
    int32_t have_pubkey = 0, new_user = 1;
    uint8_t user_pubkey[32];

    // check for the presence of an invoice id
    int64_t invoice_id_len = otxn_field(SBUF(user_pubkey), sfInvoiceID);

    TRACEVAR(invoice_id_len);
    TRACEHEX(user_pubkey);

    // ensure the invoice id can be used as a lookup key (if it has been provided)
    if (invoice_id_len == 32)
    {
        // because we also store destination tags in our state key-map we don't want to allow maliciously
        // crafted invoice_ids to cause the hook to retrieve the wrong value type from the map
        int all_zeros = 1;
        // note here we use a loop but in order to do that we need to guard the loop or it will be rejected
        for (int i = 0; GUARD(28), all_zeros && i < 28; ++i)
            if (user_pubkey[i] != 0)
                all_zeros = 0;
        if (all_zeros)
            rollback(SBUF("Liteacc: invalid public key supplied in invoice_id"), 3);

        have_pubkey = 1;
    }


    // check for the presence of a destination tag
    // with most APIs we can feed in 0,0 to return a small field as an int64_t
    int64_t dest_tag_raw = otxn_field(0, 0, sfDestinationTag);
    uint32_t dest_tag = (dest_tag_raw > 0 ? (uint32_t)(dest_tag_raw & 0xFFFFFFFFULL) : 0);

    // a user account can be credited via a destination tag but we need to look up their public key
    if (dest_tag > 0 && !have_pubkey)
    {
        // construct our dtag state lookup key (must be 256 bits)
        uint8_t keybuf[32];
        CLEARBUF(keybuf); // zero pad the left
        UINT32_TO_BUF(keybuf + 28, dest_tag);

        if (state(SBUF(user_pubkey), SBUF(keybuf)) > 0)
            have_pubkey = 1;
        else
            rollback(SBUF("Liteacc: dest tag suppled with transaction was not connected to a liteacc."), 4);
    }

    if (!have_pubkey)
        rollback(SBUF("Liteacc: Cannot continue without a lite acc public key, please supply via InvoiceID"), 5);

    // fetch the user's balance if they have one
    uint64_t user_balance = 0;
    uint8_t  user_balance_buffer[8];
    int64_t  user_balance_len = 0;
    user_balance_len = state(SBUF(user_balance_buffer), SBUF(user_pubkey));
    user_balance = UINT64_FROM_BUF(user_balance_buffer);
    if (user_balance_len > 0)
        new_user = 0;
    else
        user_balance = 0;

    // if we are dealing with an existing user then check if there is a memo attached to the transaction
    uint8_t* payment_ptr = 0;
    uint32_t payment_len = 0;
   
    if (!new_user) 
    { 
        // check for the presence of a memo
        uint8_t memos[2048];
        int64_t memos_len = otxn_field(SBUF(memos), sfMemos);
        TRACEVAR(memos_len);
        if (memos_len > 0)
        {
            // select the memodata field 
            int64_t sf = util_subfield(memos, memos_len, sfMemoData);

            // if sfMemoData was found then extract the information from the return value of the subfield api
            if (sf > 0)
            {
                payment_len = SUB_LENGTH(sf);
                payment_ptr = SUB_OFFSET(sf) + memos;
            }
        }
    }

   
    // now we compute the mode based on what was provided (as detected above)
    if (payment_ptr)
    {
        // mode 3 sending
        rollback(SBUF("Liteacc: not yet implemented"), 100);
    } 

    TRACEVAR(user_balance);
    // mode 2 receiving or mode 1 setup
    if (new_user && amount_sent < NEW_ACCOUNT_FEE_DROPS)
        rollback(SBUF("Liteacc: Insufficient drops sent to create a new account"), 6);

    if (!have_pubkey)
        rollback(SBUF("Liteacc: Must provide public key in InvoiceID field or user-id in DestTag"), 7);

    if (new_user)
        user_balance = amount_sent - NEW_ACCOUNT_FEE_DROPS;
    else
        user_balance += (amount_sent - HOOK_USAGE_FEE_DROPS);

    if (user_balance < 0)
        rollback(SBUF("Liteacc: Invariant tripped; user_balance less than 0"), 8);
    
    // encode the modified balance
    UINT64_TO_BUF(user_balance_buffer, user_balance); 
   
    TRACEVAR(user_balance);

    int64_t state_set_result = state_set(SBUF(user_balance_buffer), SBUF(user_pubkey));
    TRACEVAR(state_set_result);
    if (state_set_result < 0)
        rollback(SBUF("Liteacc: Failed to create or update user account"), 9);

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
            rollback(SBUF("Liteacc: could not assign new user a destination tag"), 10);

        // create a pointer from the dest tag to the user's public key in case we receive a payment
        // to this destination tag (so we can find out which user it is for)
        uint8_t user_dtag_key[32];
        CLEARBUF(user_dtag_key); // the first 28 bytes of the key are 0
        UINT32_TO_BUF(user_dtag_key+28, dest_tag); // the last 4 bytes of the key are the dtag
       
        if (state_set(SBUF(user_pubkey), SBUF(user_dtag_key)) < 0)
            rollback(SBUF("Liteacc: could not assign new user a destination tag"), 11);

        RBUF2(out, out_len, "Liteacc: New user's balance is ", user_balance, " and dest tag is ", dest_tag);
        accept(out, out_len, 0);
    }
    
    // format a string to return in meta
    RBUF(out, out_len, "Liteacc: User balance is ", user_balance);
    // return the string and accept the transaction
    accept(out, out_len, 0);
    return 0;
/*
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
