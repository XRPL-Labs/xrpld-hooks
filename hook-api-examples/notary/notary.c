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


int64_t cbak(int64_t reserved)
{
    return 0;
}

// maximum tx blob
#define MAX_MEMO_SIZE 4096
// LastLedgerSeq must be this far ahead of current to submit a new txn blob
#define MINIMUM_FUTURE_LEDGER 60


/**
 * Notary - easy multisign with Hooks
 * Two modes of operation:
 * 1. Attach a proposed transaction to a memo and send it to the hook account
 * 2. Endorse an already proposed transaction by using its unique ID as invoice ID and sending a 1 drop payment
 *    to the hook.
 *
 * This hook relies on the signer list on the account the hook is running on.
 * Only accounts on this list can propse and endorse multisign transactions through this Hook.
 */

int64_t hook(int64_t reserved)
{

    etxn_reserve(1);

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


    uint8_t tx_blob[MAX_MEMO_SIZE];
    int64_t tx_len = 0;
    uint8_t invoice_id[32];

    int64_t invoice_id_len = 
        otxn_field(SBUF(invoice_id), sfInvoiceID);

    // check if an invoice ID was provided... this would be mode 2 above
    if (invoice_id_len == 32)
    {
        // it was, so this is an attempt at endorsing an existing proposed multisig transaction

        // attempt to retrieve the proposed txn blob from the Hook State by setting the last nibble of the invoice ID
        // to `F` and using it as state key
        invoice_id[31] = ( invoice_id[31] & 0xF0U ) + 0x0FU;
        tx_len = state(SBUF(tx_blob), SBUF(invoice_id));
        if (tx_len < 0)
            rollback(SBUF("Notary: Received invoice id that did not correspond to a submitted multisig txn."), 1);


        // proposed txn exists... but it may have expired so we need to check that first
        int64_t  lls_lookup = sto_subfield(tx_blob, tx_len, sfLastLedgerSequence);
        uint8_t* lls_ptr = SUB_OFFSET(lls_lookup) + tx_blob;
        uint32_t lls_len = SUB_LENGTH(lls_lookup);

        if (lls_len != 4 || UINT32_FROM_BUF(lls_ptr) < ledger_seq())
        {
            // expired or invalid tx, purging
            if (state_set(0, 0, SBUF(invoice_id)) < 0)
               rollback(SBUF("Notary: Error erasing old txn blob."), 40);

            accept(SBUF("Notary: Multisig txn was too old (last ledger seq passed) and was erased."), 1);
        }
        // execution to here means the invoice ID corresponded to a currently valid proposed multisig transaction
        // that exists in the Hook State for this account
        // however we still need to check if this user is on the signer list before proceeding.
    }


    // check for the presence of a memo
    uint8_t memos[MAX_MEMO_SIZE];
    int64_t memos_len = otxn_field(SBUF(memos), sfMemos);

    uint32_t payload_len = 0;
    uint8_t* payload_ptr = 0;

    // if there is a memo present then we are in mode 1 above, but we need to ensure the user isn't invoking
    // undefined behaviour by making them pick either mode 1 or mode 2:

    if (memos_len <= 0 && invoice_id_len <= 0)
        accept(SBUF("Notary: Incoming txn with neither memo nor invoice ID, passing."), 0);

    if (memos_len > 0 && invoice_id_len > 0)
        rollback(SBUF("Notary: Incoming txn with both memo and invoice ID, abort."), 0);

    // now check if the sender is on the signer list
    // we can do this by first creating a keylet that describes the signer list on the hook account
    uint8_t keylet[34];
    CLEARBUF(keylet);
    if (util_keylet(SBUF(keylet), KEYLET_SIGNERS, SBUF(hook_accid), 0, 0, 0, 0) != 34)
        rollback(SBUF("Notary: Internal error, could not generate keylet"), 10);

    // then requesting XRPLD slot that keylet into a new slot for us
    int64_t slot_no = slot_set(SBUF(keylet), 0);
    TRACEVAR(slot_no);
    if (slot_no < 0)
        rollback(SBUF("Notary: Could not set keylet in slot"), 10);

    // once slotted we can examine the signer list object
    // the first field we are interested in is the required quorum to actually pass a multisign transaction
    int64_t result = slot_subfield(slot_no, sfSignerQuorum, 0);
    if (result < 0)
        rollback(SBUF("Notary: Could not find sfSignerQuorum on hook account"), 20);

    // we will retrieve the 4 byte quorum into a buffer, in future the will be a shortcut for this
    uint32_t signer_quorum = 0;
    uint8_t buf[4];
    result = slot(SBUF(buf), result);
    if (result != 4)
        rollback(SBUF("Notary: Could not fetch sfSignerQuorum from sfSignerEntries."), 80);
    
    // then conver the four byte buffer to an unsigned 32 bit integer
    signer_quorum = UINT32_FROM_BUF(buf);
    TRACEVAR(signer_quorum); // print the integer for debugging purposes

    // next we want to examine the signer entries, we can do this by loading the signer entries field into a new slot
    // or in this case we'll just reuse the existing slot since we're done with the parent object.
    result = slot_subfield(slot_no, sfSignerEntries, slot_no);
    if (result < 0)
        rollback(SBUF("Notary: Could not find sfSignerEntries on hook account"), 20);

    // since sfSignerEntries is an array type we can request its length with slot_count
    int64_t signer_count = slot_count(slot_no);
    if (signer_count < 0)
        rollback(SBUF("Notary: Could not fetch sfSignerEntries count"), 30);


    // now we need to iterate through all the signers in the signer entries array
    // if the account that created the originating transaction is in the list then we can pass here
    // otherwise we must rollback because the account is unauthorized
    int subslot = 0;
    uint8_t found = 0;
    uint16_t signer_weight = 0;

    for (int i = 0; GUARD(8), i < signer_count + 1; ++i)
    {
        // load the next array entry into a slot
        subslot = slot_subarray(slot_no, i, subslot);
        if (subslot < 0)
            rollback(SBUF("Notary: Could not fetch one of the sfSigner entries [subarray]."), 40);

        // load the account field from that entry into a new slot
        result = slot_subfield(subslot, sfAccount, 0);
        if (result < 0)
            rollback(SBUF("Notary: Could not fetch one of the account entires in sfSigner."), 50);

        // dump the new slot into a buffer
        uint8_t signer_account[20];
        result = slot(SBUF(signer_account), result);
        if (result != 20)
            rollback(SBUF("Notary: Could not fetch one of the sfSigner entries [slot sfAccount]."), 60);

        // load the weight field into a new slot
        result = slot_subfield(subslot, sfSignerWeight, 0);
        if (result < 0)
            rollback(SBUF("Notary: Could not fetch sfSignerWeight from sfSignerEntry."), 70);

        // dump the weight field into a buffer
        result = slot(buf, 2, result);

        if (result != 2)
            rollback(SBUF("Notary: Could not fetch sfSignerWeight from sfSignerEntry."), 80);

        // convert weight buffer to an integer
        signer_weight = UINT16_FROM_BUF(buf);

        // some debug output to see the progress
        TRACEVAR(signer_weight);
        TRACEHEX(account_field);
        TRACEHEX(signer_account);

        // compare the signer account for this signer entry against the originating transaction (sending) account
        int equal = 0;
        BUFFER_EQUAL_GUARD(equal, signer_account, 20, account_field, 20, 8);
        if (equal)
        {
            // if the otxn account was in the signer list we can stop iterating
            found = i + 1;
            break;
        }
    }

    // ensure the otxn account is authed
    if (!found)
        rollback(SBUF("Notary: Your account was not present in the signer list."), 70);

    // execution to this point means the following:
    // 1. the originating transaction (sending) account is authorized as one of the signers on the hook account
    // 2. either an invoice ID or a memo was sent to the hook (but not both).

    // if a memo was sent to the hook it must be mode 1 above (proposing a new multisign transaction)
    if (memos_len > 0)
    {
        // this is a defensive check, it is actually never executed due to an identical condition above
        if (invoice_id_len > 0)
            rollback(SBUF("Notary: Incoming transaction with both invoice id and memo. Aborting."), 0);

        // since our memos are in a buffer inside the hook (as opposed to being a slot) we use the sto api with it
        // the sto apis probe into a serialized object returning offsets and lengths of subfields or array entries
        int64_t   memo_lookup = sto_subarray(memos, memos_len, 0);
        uint8_t*  memo_ptr = SUB_OFFSET(memo_lookup) + memos;
        uint32_t  memo_len = SUB_LENGTH(memo_lookup);

        // memos are nested inside an actual memo object, so we need to subfield
        // equivalently in JSON this would look like memo_array[i]["Memo"]
        memo_lookup = sto_subfield(memo_ptr, memo_len, sfMemo);
        memo_ptr = SUB_OFFSET(memo_lookup) + memo_ptr;
        memo_len = SUB_LENGTH(memo_lookup);

        if (memo_lookup < 0)
            rollback(SBUF("Notary: Incoming txn had a blank sfMemos, abort."), 1);

        int64_t  format_lookup   = sto_subfield(memo_ptr, memo_len, sfMemoFormat);
        uint8_t* format_ptr = SUB_OFFSET(format_lookup) + memo_ptr;
        uint32_t format_len = SUB_LENGTH(format_lookup);

        int is_unsigned_payload = 0;
        BUFFER_EQUAL_STR_GUARD(is_unsigned_payload, format_ptr, format_len,     "unsigned/payload+1",   1);
        if (!is_unsigned_payload)
            accept(SBUF("Notary: Memo is an invalid format. Passing txn."), 50);

        int64_t  data_lookup = sto_subfield(memo_ptr, memo_len, sfMemoData);
        uint8_t* data_ptr = SUB_OFFSET(data_lookup) + memo_ptr;
        uint32_t data_len = SUB_LENGTH(data_lookup);

        if (data_len > MAX_MEMO_SIZE)
            rollback(SBUF("Notary: Memo too large (4kib max)."), 4);

        // inspect unsigned payload
        // first check that sfTransactionType appears in the memo... if it doesn't then it can't be a transaction
        int64_t txtype_lookup      = sto_subfield(data_ptr, data_len, sfTransactionType);
        if (txtype_lookup < 0)
            rollback(SBUF("Notary: Memo is invalid format. Should be an unsigned transaction."), 2);

        // next check the lastLedgerSequence is sensibly set otherwise there will be no chance for the other signers
        // to endorse the txn before it expires
        int64_t  lls_lookup = sto_subfield(data_ptr, data_len, sfLastLedgerSequence);
        uint8_t* lls_ptr = SUB_OFFSET(lls_lookup) + data_ptr;
        uint32_t lls_len = SUB_LENGTH(lls_lookup);

        // check for expired txn
        if (lls_len != 4 || UINT32_FROM_BUF(lls_ptr) < ledger_seq() + MINIMUM_FUTURE_LEDGER)
            rollback(SBUF("Notary: Provided txn blob expires too soo (LastLedgerSeq)."), 3);

        // compute txn hash, this becomes the ID passed as an invoice ID by the endorsers (other signers)
        if (util_sha512h(SBUF(invoice_id), data_ptr, data_len) < 0)
            rollback(SBUF("Notary: Could not compute sha512 over the submitted txn."), 5);

        TRACEHEX(invoice_id);

        invoice_id[31] = ( invoice_id[31] & 0xF0U ) + 0x0FU;

        // write blob to state... the state key for the txn blob is the txn ID with `F` as the last nibble.
        if (state_set(data_ptr, data_len, SBUF(invoice_id)) != data_len)
            rollback(SBUF("Notary: Could not write txn to hook state."), 6);

    }

    // execution to here means if we were in mode 1 we now drop into mode 2, because the proposed txn is now recorded
    // so we simply treat this as an endorsement (mode 2) from here...

    // record the signature... the state key for this is the txn ID with (1 + signer number) as the last nibble
    invoice_id[31] = ( invoice_id[31] & 0xF0U ) + found;
    
    // the value we record against the signer is his/her signer weight at the time the endorsement or proposal happened
    UINT16_TO_BUF(buf, signer_weight);
    if (state_set(buf, 2, SBUF(invoice_id)) != 2)
        rollback(SBUF("Notary: Could not write signature to hook state."), 7);
    

    // check if we have managed to achieve a quorum by loading all current signatures and adding together the signer
    // weights (stored as the HookState values)
    uint32_t total = 0;
    for (uint8_t i = 1; GUARD(8), i < 9; ++i)
    {
        invoice_id[31] = ( invoice_id[31] & 0xF0U ) + i;
        if (state(buf, 2, SBUF(invoice_id)) == 2)
            total += UINT16_FROM_BUF(buf);
    }

    TRACEVAR(total);
    TRACEVAR(signer_quorum);

    // if we haven't achieved a quorum we will output the ID as the hook result string so it can be given to the
    // other endorsers
    if (total < signer_quorum)
    {
        uint8_t header[] = "Notary: Accepted waiting for other signers...: ";
        uint8_t returnval[112]; 
        uint8_t* ptr = returnval;
        for (int i = 0; GUARD(47), i < 47; ++i)
            *ptr++ = header[i];
        for (int i = 0; GUARD(32),i < 32; ++i)
        {
            uint8_t hi = (invoice_id[i] >> 4U);
            uint8_t lo = (invoice_id[i] & 0xFU);

            hi += ( hi > 9 ? ('A'-10) : '0' );
            lo += ( lo > 9 ? ('A'-10) : '0' );
            *ptr++ = hi;
            *ptr++ = lo;
        }
        accept(SBUF(returnval), 0);
    }

    // execution to here means we achieved a quorum on a proposed txn
    // therefore we must now emit the txn then garbage collect the old state
    int should_emit = 1;
    invoice_id[31] = ( invoice_id[31] & 0xF0U ) + 0x0FU;
    tx_len = state(SBUF(tx_blob), SBUF(invoice_id));
    if (tx_len < 0)
        should_emit = 0;

    // delete everything from state before emitting
    state_set(0, 0, SBUF(invoice_id));
    for (uint8_t i = 1; GUARD(8), i < 9; ++i)
    {
        invoice_id[31] = ( invoice_id[31] & 0xF0U ) + i;
        state_set(0, 0, SBUF(invoice_id));
    }
    
    if (!should_emit)
        rollback(SBUF("Notary: Tried to emit multisig txn but it was msising"), 1);

    // blob exists, check expiry
    int64_t  lls_lookup = sto_subfield(tx_blob, tx_len, sfLastLedgerSequence);
    uint8_t* lls_ptr = SUB_OFFSET(lls_lookup) + tx_blob;
    uint32_t lls_len = SUB_LENGTH(lls_lookup);

    if (lls_len != 4 || UINT32_FROM_BUF(lls_ptr) < ledger_seq())
        rollback(SBUF("Notary: Was about to emit txn but it's too old now"), 1);

    // modify the txn for emission
    // we need to remove sfSigners if it exists
    // we need to zero sfSequence sfSigningPubKey and sfTxnSignature
    // we need to correctly set sfFirstLedgerSequence

    // first do the erasure, this can fail if there is no such sfSigner field, so swap buffers to immitate success
    
    uint8_t  buffer[MAX_MEMO_SIZE];
    uint8_t* buffer2 = buffer;
    uint8_t* buffer1 = tx_blob;

    result = sto_erase(buffer2, MAX_MEMO_SIZE, buffer1, tx_len, sfSigners);
    if (result > 0)
        tx_len = result;
    else
        BUFFER_SWAP(buffer1, buffer2);

    // next zero sfSequence
    uint8_t zeroed[6];
    CLEARBUF(zeroed);
    zeroed[0] = 0x24U; // this is the lead byte for sfSequence

    tx_len = sto_emplace(buffer1, MAX_MEMO_SIZE, buffer2, tx_len, zeroed, 5, sfSequence);
    if (tx_len <= 0)
        rollback(SBUF("Notary: Emplacing sfSequence failed."), 1);


    // next set sfTxnSignature to 0
    zeroed[0] = 0x74U; // lead byte for sfTxnSignature, next byte is length which is 0
    tx_len = sto_emplace(buffer2, MAX_MEMO_SIZE, buffer1, tx_len, zeroed, 2, sfTxnSignature);
    TRACEVAR(tx_len);
    if (tx_len <= 0)
        rollback(SBUF("Notary: Emplacing sfTxnSignature failed."), 1);
    
    // next set sfSigningPubKey to 0
    zeroed[0] = 0x73U;  // this is the lead byte for sfSigningPubkey, note that the next byte is 0 which is the length
    tx_len = sto_emplace(buffer1, MAX_MEMO_SIZE, buffer2, tx_len, zeroed, 2, sfSigningPubKey);
    TRACEVAR(tx_len);
    if (tx_len <= 0)
        rollback(SBUF("Notary: Emplacing sfSigningPubKey failed."), 1);

    // finally set FirstLedgerSeq appropriately
    uint32_t fls = ledger_seq() + 1;
    zeroed[0] = 0x20U;
    zeroed[1] = 0x1AU;
    UINT32_TO_BUF(zeroed + 2, fls);
    tx_len = sto_emplace(buffer2, MAX_MEMO_SIZE, buffer1, tx_len, zeroed, 6, sfFirstLedgerSequence);
    
    if (tx_len <= 0)
        rollback(SBUF("Notary: Emplacing sfFirstLedgerSequence failed."), 1);

    // finally add emit details
    uint8_t emitdet[105];
    result = etxn_details(emitdet, 105);

    if (result < 0)
        rollback(SBUF("Notary: EmitDetails failed to generate."), 1);

    tx_len = sto_emplace(buffer1, MAX_MEMO_SIZE, buffer2, tx_len, SBUF(emitdet), sfEmitDetails);
    if (tx_len < 0)
        rollback(SBUF("Notary: Emplacing sfEmitDetails failed."), 1);

    // replace fee with something currently appropriate
    uint8_t fee[ENCODE_DROPS_SIZE];
    uint8_t* fee_ptr = fee; // this ptr is incremented by the macro, so just throw it away
    int64_t fee_to_pay = etxn_fee_base(tx_len + ENCODE_DROPS_SIZE);
    ENCODE_DROPS(fee_ptr, fee_to_pay, amFEE);
    tx_len = sto_emplace(buffer2, MAX_MEMO_SIZE, buffer1, tx_len, SBUF(fee), sfFee);
    
    if (tx_len <= 0)
        rollback(SBUF("Notary: Emplacing sfFee failed."), 1);
    
    uint8_t emithash[32];
    if (emit(SBUF(emithash), buffer2, tx_len) < 0)
        accept(SBUF("Notary: All conditions met but emission failed: proposed txn was malformed."), 1);

    accept(SBUF("Notary: Emitted multisigned txn"), 0);
    return 0;
}

