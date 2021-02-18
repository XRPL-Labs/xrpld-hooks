/**
 * Notary.c - An example hook for collecting signatures for multi-sign transactions without blocking sequence number
 * on the account. 
 *
 * Author: Richard Holland
 * Date: 11 Feb 2021
 *
 **/

#include <stdint.h>
#include "../hookapi.h"

/**
 * RH TODO
 *  - handle a callback
 *  - rollback a send (minus a fee) if callback doesnt trigger within X ledgers
 */

int64_t cbak(int64_t reserved)
{
    accept(0,0,0);
    return 0;
}

#define MAX_MEMO_SIZE 4096

/**
sto_erase( ... sfLastLedgerSequence )
sto_erase( ... sfFirstLedgerSequence)
sto_erase( ... sfSequence )
sto_erase( ... sfTxnSignature )
sto_erase( ... sfSigningPubkey )
sto_erase( ... sfSigners )
*/

int64_t hook(int64_t reserved)
{

    // this api fetches the AccountID of the account the hook currently executing is installed on
    // since hooks can be triggered by both incoming and ougoing transactions this is important to know
    unsigned char hook_accid[20];
    hook_account((uint32_t)hook_accid, 20);


    // next fetch the sfAccount field from the originating transaction
    uint8_t account_field[20];
    int32_t account_field_len = otxn_field(SBUF(account_field), sfAccount);
    if (account_field_len < 20)                                   // negative values indicate errors from every api
        rollback(SBUF("Notary: sfAccount field missing!!!"), 10); // this code could never be hit in prod
                                                                  // but it's here for completeness

    // compare the "From Account" (sfAccount) on the transaction with the account the hook is running on
    int equal = 0; BUFFER_EQUAL(equal, hook_accid, account_field, 20);
    if (equal)
        accept(SBUF("Notary: Outgoing transaction"), 20);

    // TODO: Slots: get signerlist
    // for each 1 drop payment sent by an account to the hook as per the below modes of operation
    // add a memory of that signature
    // when signerlist is satisfied by 1 drop payments, 

    /**
     * Notary Modes of operation: 
     * ------------------------------
     *  1. New Tx  - Txn Blob in Memo unsigned/payload+1
     *               adds a new tx to the state for signature collection
     *  2. Add Sig - Txn Hash in Invoice ID
     *               adds a signature to an existing tx already in state, or if the subject txn has expired removes it
     **/

    int64_t invoice_id_len = otxn_field(SBUF(user_pubkey), sfInvoiceID);

    // ensure the invoice id can be used as a lookup key (if it has been provided)
    if (invoice_id_len == 32)
    // check for the presence of a memo
    uint8_t memos[4096];
    int64_t memos_len = otxn_field(SBUF(memos), sfMemos);

    uint32_t payload_len = 0;
    uint8_t* payload_ptr = 0;

    
    if (memos_len <= 0)
        accept(SBUF("Notary: Incoming transaction without a memo. Passing txn."), 0);

    /**
     * Memo: { MemoData: <app data>,   MemoFormat: "unsigned/payload+1",   MemoType: <anything> }
     **/

    // the memos are presented in an array object, which we must index into
    int64_t memo_lookup = util_subarray(memos, memos_len, i);

    TRACEVAR(memo_lookup);
    if (memo_lookup < 0)
        break; // invalid or too few memos

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
    int64_t data_lookup     = util_subfield(memo_ptr, memo_len, sfMemoData);
    int64_t format_lookup   = util_subfield(memo_ptr, memo_len, sfMemoFormat);

    // if any of these lookups fail the request is malformed
    if (data_lookup < 0 || format_lookup < 0)
        break;

    // care must be taken to add the correct pointer to an offset returned by sub_array or sub_field
    // since we are working relative to the specific memo we must add memo_ptr, NOT memos or something else
    uint8_t* data_ptr = SUB_OFFSET(data_lookup) + memo_ptr;
    uint32_t data_len = SUB_LENGTH(data_lookup);

    uint8_t* format_ptr = SUB_OFFSET(format_lookup) + memo_ptr;
    uint32_t format_len = SUB_LENGTH(format_lookup);

    int is_unsigned_payload = 0;
    BUFFER_EQUAL_STR_GUARD(is_unsigned_payload, format_ptr, format_len,     "unsigned/payload+1",   1);
    if (!is_unsigned_payload)
        accept(SBUF("Notary: Memo is an invalid format. Passing txn."), 50);
    
    int64_t txhash_lookup      = util_subfield(payload_ptr, payload_len, sfTransactionHash);
    int64_t txtype_lookup      = util_subfield(payload_ptr, payload_len, sfTransactionType);

    uint8_t* txhash_ptr = (txhash_lookup > 0 ? SUB_OFFSET(txhash_lookup) + payload_ptr : 0);
    uint32_t txhash_len = (txhash_lookup > 0 ? SUB_LENGTH(txhash_lookup) : 0);

    uint8_t* txtype_ptr = (txtype_lookup > 0 ? SUB_OFFSET(txtype_lookup) + payload_ptr : 0);
    uint32_t txtype_len = (txtype_lookup > 0 ? SUB_LENGTH(txtype_lookup) : 0);

    // Notary MODE differentiation:
    // A = txhash present, B = txtype present, C = signer present
    // Mode 1 New Transaction   : !A &&  B &&  C
    // Mode 2 Add Signature     :  A && !B &&  C
    // Mode 3 Prune Expired     :  A && !B && !C
    
    if ( !txhash_ptr && txtype_ptr && signer_ptr ) 
    {
        // mode 1 new transaction
        // validate before continuing
        int64_t lls_lookup = util_subfield(payload_ptr, payload_len, sfLastLedgerSequence);
        int64_t fls_lookup = util_subfield(payload_ptr, payload_len, sfLastLedgerSequence);
        if (fls_lookup >= 0)
            rollback(SBUF("Notary: FirstLedgerSequence not supported."), 1);
        if (lls_lookup < 0)
            rollback(SBUF("Notary: Transaction lacked LastLedgerSequence."), 1);

        uint8_t* lls_ptr = SUB_OFFSET(lls_lookup) + payload_ptr;
        uint32_t lls_len = SUB_LENGTH(lls_lookup);

        if (lls_len != 4 || UINT32_FROM_BUF(lls_ptr) < ledger_seq() + 60)
            rollback(SBUF("Notary: Will not accept tx with less than 60 ledgers to live."), 1);

        int64_t seq_lookup = util_subfield(payload_ptr, payload_len, sfSequence);
        uint8_t* seq_ptr = SUB_OFFSET(seq_lookup) + payload_ptr;
        uint32_t seq_len = SUB_LENGTH(seq_lookup);
        if (seq_lookup < 0 || (seq_lookup >= 0 && (seq_len != 4 || UINT32_FROM_BUF(seq_ptr) != 0)))
            rollback(SBUF("Notary: Will not accept txn without sfSequence = 0."), 1);
   
        // this lookup checks for sfTxnSignature on the top level transaction, as distinct from signer_sig_lookup
        // which is the same field code/type but inside the sfSigner structure. the top level sfTxnSignature must
        // be all 0's. 
        int64_t sig_lookup = util_subfield(payload_ptr, payload_len, sfTxnSignature);
        uint8_t* sig_ptr = SUB_OFFSET(sig_lookup) + payload_ptr;
        uint32_t sig_len = SUB_LENGTH(sig_lookup);
        // RH TODO: check more than just the first four bytes of sig_ptr are zero
        if (sig_lookup < 0 || (sig_lookup >= 0 && (sig_len != 32 || UINT32_FROM_BUF(sig_ptr) != 0 )))
            rollback(SBUF("Notary: Will not accept txn without sfTxnSignature = 0."), 1);
        

        

        // we must enforce blank signingKey, force the txn to be submitted by 
        uint8_t hash[32];
        util_sha512h(SBUF(hash), payload_ptr, payload_len);
    }
    else if ( txhash_ptr && !txtype_ptr && signer_ptr )
    {
        // mode 2

    }
    else if ( txhash_ptr && !txtype_ptr && !signer_ptr ) 
    {
        // mode 3

    }
    else
    {
        // invalid mode
        rollback(SBUF("Notary: Invalid mode."), 1);
    }


    uint8_t* pubkey_ptr = SUB_OFFSET(pubkey_lookup) + payload_ptr;
    uint32_t pubkey_len = SUB_LENGTH(pubkey_lookup);

    if (txhash_lookup > 0)
    {
        // mode 2 add signature
        uint8_t* txhash_ptr = SUB_OFFSET(txhash_lookup) + payload_ptr;
        uint32_t txhash_len = SUB_LENGTH(txhash_lookup);



    }


    // execution to here means they are actually trying to interact with the hook so we will begin rolling back
    // on error instead of passing txns

    // remove expired mode 3
    /*
    {
        // the user should have passed exactly 32 bytes as an unsigned payload to prompt the hook to lookup and
        // remove a potentially expired transaction from its state
        if (payload_len != 32)
            rollback(SBUF("Notary: Provided unsigned payload must be 32 bytes (tx hash)"), 1);
        
        // erase the last nibble of the transaction hash
        payload_ptr[31] &= 0xF0U;

        // set the last nibble to F to refer to the tx blob's state key
        payload_ptr[31] |= 0x0FU;

        // RH TODO: review max tx size limit
        uint8_t tx_ptr[MAX_MEMO_SIZE];
        int64_t tx_len = state(SBUF(tx_ptr), payload_ptr, payload_len);

        if (tx_len < 1)
            rollback(SBUF("Notary: No tx by that hash currently recorded."), 100);

        // the transaction exists in our state but we will need to parse the sfLastLedgerSequence out of it
        int64_t expiry_lookup = util_subfield(tx_ptr, tx_len, sfLastLedgerSequence);
        if (expiry_lookup > 0)
        {
            uint8_t* expiry_ptr = SUB_OFFSET(expiry_lookup) + tx_ptr;
            uint32_t expiry_len = SUB_LENGTH(expiry_lookup);
            if (expiry_len == 4 &&
                    UINT32_FROM_BUF(expiry_ptr) > ledger_seq())
               rollback(SBUF("Notary: That tx has not yet expired"), 1);
        }

        // execution to here means the tx did expire or is somehow otherwise invalid, so remove it

        // to remove we simply pass 0,0 as the ptr and len
        if (state_set(0, 0, payload_ptr, payload_len) < 0)
            rollback(SBUF("Notary: Failed to remove expired txn"), 1);

        // additionally remove any signatures which were submitted
        for (uint8_t i = 0; GUARD(8), i < 8; ++i)
        {
            // erase the last nibble of the transaction hash
            payload_ptr[31] &= 0xF0U;
            // set the last nibble to i
            payload_ptr[31] |= i;
            state_set(0, 0, payload_ptr, payload_len); // we don't care if this fails or succeeds
        }

        // done!
        accept(SBUF("Notary: Removed expired txn"), 1);
        return 0;
    }
    */

    // execution to here means they are either submitting a new txn (mode 1) or adding a signature (mode 2)

    // check the signature is valid
    /*
    if (!util_verify(payload_ptr,    payload_len,
                     signature_ptr,  signature_len,
                     publickey_ptr,  publickey_len))
        rollback(SBUF("Notary: Invalid signature in memo."), 60);
    */


    // RH UPTO HERE

    

    /*

    // we now need to confirm the details of the requested transaction against our state objects
    // lookup their source tag in our hook state
    uint8_t state_request[32];
    CLEARBUF(state_request);                        // set every byte to 0
    UINT32_TO_BUF(state_request + 28,  src_tag);    // set the last 4 bytes to src_tag

    // 'all zeros' ending in the source tag should retun the user's public key
    uint8_t pubkey_from_state[32];
    if (state(SBUF(pubkey_from_state), SBUF(state_request)) != 32)
        rollback(SBUF("Notary: [3] No lite account was associated with the supplied source tag."), 100);

    // ... and that public key on record should match the public key supplied in the memo
    int is_equal = 0;
    BUFFER_EQUAL(is_equal, pubkey_from_state, pubkey_from_memo + 1 , 32);
    if (!is_equal)
        rollback(SBUF("Notary: [3] Src tag did not match public key on file for this account."), 110);

    // we also need to lookup the user's sequence number to prevent replay attacks
    // to do that we take the same key we used for the source_tag->publickey lookup and change the first
    // four bytes to be 0xFF
    UINT32_TO_BUF(state_request, 0xFFFFFFFFUL);
    uint8_t last_seq_buf[4];
    if (state(SBUF(last_seq_buf), SBUF(state_request)) != 4)
        rollback(SBUF("Notary: [3] Last sequence not found in lookup."), 120);

    // extract the sequence number from the returned state
    uint32_t last_seq = UINT32_FROM_BUF(last_seq_buf);
    if (last_seq >= seq)
        rollback(SBUF("Notary: [3] Last sequence is >= provided sequence number."), 130);

    // update the sequence number
    CLEARBUF(last_seq_buf);
    UINT32_TO_BUF(last_seq_buf, seq);
    if (state_set(SBUF(last_seq_buf), SBUF(state_request)) != 4)
        rollback(SBUF("Notary: [3] Could not set new sequence number on lite account."), 135);

    // finally lookup user's balance
    uint8_t balance_buf[8];
    if (state(SBUF(balance_buf), SBUF(pubkey_from_state)) != 8)
        rollback(SBUF("Notary: [3] Could not retrieve user's balance."), 140);

    // EDGE case: a lite user pays another lite account
    // in this case the destination is the hook account which would fail if emitted
    int lite2lite = 0;
    BUFFER_EQUAL(lite2lite, hook_accid, destination, 20);

    // prepare to emit a transaction
    if (!lite2lite)
        etxn_reserve(1);

    int64_t fee_base = lite2lite ? 0 : etxn_fee_base(PREPARE_PAYMENT_SIMPLE_SIZE);

    // calculate the total cost to the user and make sure they can pay
    uint64_t billable = drops_to_send + ADDITIONAL_SEND_FEE_DROPS + HOOK_USAGE_FEE_DROPS +
        ( SUBSIDIZED_SENDING ? 0 : fee_base );

    if (billable <= 0 || billable < drops_to_send)
        rollback(SBUF("Notary: [3] Invariant tripped."), 145);

    uint64_t balance = UINT64_FROM_BUF(balance_buf);
    if (balance < billable)
    {
        RBUF2(out, out_len, "Notary: [3] User balance ", balance, " less than required ", billable);
        rollback(out, out_len, 150);
    }

    uint64_t new_balance = balance - billable;

    // defensive sanity check
    if (balance <= new_balance)
        rollback(SBUF("Notary: [3] Invariant tripped."), 160);

    CLEARBUF(balance_buf);

    // write balance
    UINT64_TO_BUF(balance_buf, new_balance);
    if (state_set(SBUF(balance_buf), SBUF(pubkey_from_state)) != 8)
        rollback(SBUF("Notary: [3] Failed to set new balance for user."), 170);

    // don't emit a transaction for lite to lite transfers
    if (lite2lite)
    {
        // lookup destination tag
        uint8_t state_request[32];
        CLEARBUF(state_request);
        UINT32_TO_BUF(state_request + 28, dest_tag);

        uint8_t destpk_buf[32];
        if (state(SBUF(destpk_buf), SBUF(state_request)) != 32)
            rollback(SBUF("Notary: [3] Destination tag did not match any user (lite2lite xfer.)"), 180);

        // look up dest user balance
        CLEARBUF(balance_buf);
        if (state(SBUF(balance_buf), SBUF(destpk_buf)) != 8)
            rollback(SBUF("Notary: [3] Could not fetch destination balance (lite2lite xfer.)"), 190);

        uint64_t dest_balance = UINT64_FROM_BUF(balance_buf);

        // compute new balance
        uint64_t new_dest_balance = dest_balance + drops_to_send;

        if (new_dest_balance < dest_balance)
            rollback(SBUF("Notary: [3] Invariant tripped."), 200);

        CLEARBUF(balance_buf);
        UINT64_TO_BUF(balance_buf, new_dest_balance);

        // set new balance
        if (state_set(SBUF(balance_buf), SBUF(destpk_buf)) != 8)
            rollback(SBUF("Notary: [3] Could not set destination balance (lite2lite xfer.)"), 210);

        RBUF2(out, out_len, "Notary: [3] Successful l2l xfer ", drops_to_send, ", new balance: ", new_balance);
        accept(out, out_len, 0);
    }


    // create a buffer to write the emitted transaction into
    unsigned char tx[PREPARE_PAYMENT_SIMPLE_SIZE];

    // we will use an XRP payment macro, this will populate the buffer with a serialized binary transaction
    // Parameter list: ( buf_out, drops_amount, drops_fee, to_address, dest_tag, src_tag )
    PREPARE_PAYMENT_SIMPLE(tx, drops_to_send, fee_base, destination, dest_tag, src_tag);

    // emit the transaction
    emit(SBUF(tx));

    // accept the originating transaction, this will cause the state updates to propagate and the emitted tx
    // to enter the transaction queue atomically.
    RBUF2(out, out_len, "Notary: [3] Successfully emitted ", drops_to_send, ", new balance: ", new_balance);
    accept(out, out_len, 0);
    // execution will not occur past here

    // --------- MODE [1] and [2] ---------
    // execution to here means no memo was attached to the originating transaction, so we are either mode [1] or [2]

    // collect information from the invoice ID and destination tags...
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
            rollback(SBUF("Notary: [0] invalid public key supplied in invoice_id"), 180);

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
            rollback(SBUF("Notary: [2] dest tag suppled with transaction was not connected to a notary."), 190);
    }

    // by this point in the hook we should definitely have a public key for the lite account, either because
    // the user supplied it in the invoice id or because the user supplied a destination tag which we looked up
    if (!have_pubkey)
        rollback(
            SBUF("Notary: [1/2] Cannot continue without a lite acc public key, please supply via InvoiceID"), 200);

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

    if (new_user && amount_sent < NEW_ACCOUNT_FEE_DROPS)
        rollback(SBUF("Notary: [1] Insufficient drops sent to create a new account."), 210);

    if (!have_pubkey)
        rollback(SBUF("Notary: [1|2] Must provide public key in InvoiceID field or user-id in DestTag."), 220);

    if (new_user)
        user_balance = amount_sent - NEW_ACCOUNT_FEE_DROPS;
    else
        user_balance += (amount_sent - HOOK_USAGE_FEE_DROPS);

    if (user_balance < 0)
        rollback(SBUF("Notary: [1|2] Invariant tripped; user_balance less than 0."), 230);

    // encode the modified balance
    UINT64_TO_BUF(user_balance_buffer, user_balance);

    int64_t state_set_result = state_set(SBUF(user_balance_buffer), SBUF(user_pubkey));
    if (state_set_result < 0)
        rollback(SBUF("Notary: [1|2] Failed to create or update user account."), 240);

    if (!new_user)
    {
        // if it's an existing user we're done, accept the originating transaction and apply state updates
        RBUF(out, out_len, "Notary: [2] User balance is ", user_balance);
        accept(out, out_len, 0);
        // execution stops here
    }

    // execution to here means we have a new user so set up their other state fields

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

    // update the counter's buffer to reflect the incremented value
    UINT32_TO_BUF(dtag_counter_buffer, dest_tag);

    // update the state for counter
    if (state_set(SBUF(dtag_counter_buffer), SBUF(dtag_counter_key)) < 0)
        rollback(SBUF("Notary: [1] Could not assign new user a destination tag."), 250);

    // create a map entry from the dest tag to the user's public key in case we receive a payment
    // to this destination tag (so we can find out which user it is for)
    uint8_t state_key[32];
    CLEARBUF(state_key); // the first 28 bytes of the key are 0
    UINT32_TO_BUF(state_key+28, dest_tag); // the last 4 bytes of the key are the dtag

    if (state_set(SBUF(user_pubkey), SBUF(state_key)) < 0)
        rollback(SBUF("Notary: [1] Could not assign new user a destination tag."), 260);

    // create the user's sequence number entry (this is updated when mode 3 is used)
    uint8_t blank[4];
    CLEARBUF(blank);
    UINT32_TO_BUF(state_key, 0xFFFFFFFFUL);
    if (state_set(SBUF(blank), SBUF(state_key)) < 0)
        rollback(SBUF("Notary: [1] Could not assign new user a sequence number."), 270);

    // accept originating transaction and apply state changes
    RBUF2(out, out_len, "Notary: [1] New user's balance is ", user_balance, " and dest tag is ", dest_tag);
    accept(out, out_len, 0);
    // execution stops here
*/
    return 0;
}
