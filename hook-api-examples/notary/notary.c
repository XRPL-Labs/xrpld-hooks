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

// maximum tx blob
#define MAX_MEMO_SIZE 4096
// LastLedgerSeq must be this far ahead of current to submit a new txn blob
#define MINIMUM_FUTURE_LEDGER 60
//
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

    uint8_t tx_blob[MAX_MEMO_SIZE];

    uint8_t invoice_id[32];
    int64_t invoice_id_len = otxn_field(SBUF(invoice_id), sfInvoiceID);
    if (invoice_id_len == 32)
    {
        // select the blob by setting the last nibble to F
        invoice_id[31] = ( invoice_id[31] & 0xF0U ) + 0x0FU;
        if (state(SBUF(tx_blob), SBUF(invoice_id)) > 0)
        {
            // blob exists, check expiry
            int64_t  lls_lookup = util_subfield(SBUF(tx_blob), sfLastLedgerSequence);
            uint8_t* lls_ptr = SUB_OFFSET(lls_lookup) + tx_blob;
            uint32_t lls_len = SUB_LENGTH(lls_lookup);

            if (lls_len != 4 || UINT32_FROM_BUF(lls_ptr) < ledger_seq())
            {
                // expired or invalid tx, purging
                if (state_set(0, 0, SBUF(invoice_id)) < 0)
                   rollback(SBUF("Notary: Error erasing old txn blob."), 40);
                accpet(SBUF("Notary: Multisig txn was too old (last ledger seq passed) and was erased."), 1);
            }
        }
    }

    // check for the presence of a memo
    uint8_t memos[MAX_MEMO_SIZE];
    int64_t memos_len = otxn_field(SBUF(memos), sfMemos);

    uint32_t payload_len = 0;
    uint8_t* payload_ptr = 0;

    if (memo_len <= 0 && invoice_id_len <= 0)
        accept(SBUF("Notary: Incoming txn with neither memo nor invoice ID, passing."), 0);

    if (memo_len > 0 && invoice_id_len > 0)
        rollback(SBUF("Notary: Incoming txn with both memo and invoice ID, abort."), 0);

    // now check if the sender is on the signer list
    uint8_t keylet[34];
    CLEARBUF(keylet);
    if (util_keylet(SBUF(keylet), KEYLET_SIGNERS, SBUF(hook_accid), 0, 0, 0, 0) != 34)
        rollback(SBUF("Notary: Internal error, could not generate keylet"), 10);

    int64_t slot_no = slot_set(SBUF(keylet), 0);
    TRACEVAR(slot_no);
    if (slot_no < 0)
        rollback(SBUF("Notary: Could not set keylet in slot"), 10);

    int64_t result = slot_subfield(slot_no, sfSignerEntries, slot_no);
    if (result < 0)
        rollback(SBUF("Notary: Could not find sfSignerEntries on hook account"), 20);

    int64_t signer_count = slot_count(slot_no);
    if (signer_count < 0)
        rollback(SBUF("Notary: Could not fetch sfSignerEntries count"), 30);


    int64_t result = slot_subfield(slot_no, sfSignerQuorum, 0);
    if (result < 0)
        rollback(SBUF("Notary: Could not find sfSignerQuoprum on hook account"), 20);

    uint32_t signer_quorum = 0;
    result = slot(&signer_quorum, 4, result);
    if (result != 4)
        rollback(SBUF("Notary: Could not fetch sfSignerQuorum from sfSignerEntries."), 80);

    int subslot = 0;
    uint8_t found = 0;
    uint16_t signer_weight = 0;

    for (int i = 0; GUARD(8), i < signer_count; ++i)
    {
        subslot = slot_subarray(slot_no, 0, subslot);
        if (sublot < 0)
            rollback(SBUF("Notary: Could not fetch one of the sfSigner entries [subarray]."), 40);

        result = slot_subfield(subslot, sfAccount, 0);
        if (result < 0)
            rollback(SBUF("Notary: Could not fetch one of the account entires in sfSigner."), 50);

        uint8_t signer_account[20];
        result = slot(SBUF(signer_account), result);
        if (result != 20)
            rollback(SBUF("Notary: Could not fetch one of the sfSigner entries [slot sfAccount]."), 60);

        result = slot_subfield(subslot, sfSignerWeight, 0);
        if (result < 0)
            rollback(SBUF("Notary: Could not fetch sfSignerWeight from sfSignerEntry."), 70);

        result = slot(&signer_weight, 2, result);

        if (result != 2)
            rollback(SBUF("Notary: Could not fetch sfSignerWeight from sfSignerEntry."), 80);

        int equal = 0;
        BUFFER_EQUAL_GUARD(equal, signer_account, 20, account_field, 20, 8);

        if (equal)
        {
            found = i + 1;
            break;
        }
    }

    if (!found)
        rollback(SBUF("Notary: Your account was not present in the signer list."), 70);

    if (memo_len)
    {
        if (invoice_id_len > 0)
            rollback(SBUF("Notary: Incoming transaction with both invoice id and memo. Aborting."), 0);

        int64_t   memo_lookup = util_subarray(memos, memos_len, 0);
        uint8_t*  memo_ptr = SUB_OFFSET(memo_lookup) + memos;
        uint32_t  memo_len = SUB_LENGTH(memo_lookup);

        trace(SBUF("MEMO:"), 0);
        trace(memo_ptr, memo_len, 1);

        // memos are nested inside an actual memo object, so we need to subfield
        // equivalently in JSON this would look like memo_array[i]["Memo"]
        memo_lookup = util_subfield(memo_ptr, memo_len, sfMemo);
        memo_ptr = SUB_OFFSET(memo_lookup) + memo_ptr;
        memo_len = SUB_LENGTH(memo_lookup);

        if (memo_lookup < 0)
            rollback(SBUF("Notary: Incoming txn had a blank sfMemos, abort."), 1);

        int64_t  format_lookup   = util_subfield(memo_ptr, memo_len, sfMemoFormat);
        uint8_t* format_ptr = SUB_OFFSET(format_lookup) + memo_ptr;
        uint32_t format_len = SUB_LENGTH(format_lookup);

        int is_unsigned_payload = 0;
        BUFFER_EQUAL_STR_GUARD(is_unsigned_payload, format_ptr, format_len,     "unsigned/payload+1",   1);
        if (!is_unsigned_payload)
            accept(SBUF("Notary: Memo is an invalid format. Passing txn."), 50);

        int64_t  data_lookup = util_subfield(memo_ptr, memo_len, sfMemoData);
        uint8_t* data_ptr = SUB_OFFSET(data_lookup) + memo_ptr;
        uint32_t data_len = SUB_LENGTH(data_lookup);

        if (data_len > MAX_MEMO_SIZE)
            rollback(SBUF("Notary: Memo too large (4kib max)."), 4);

        // inspect unsigned payload
        int64_t txtype_lookup      = util_subfield(payload_ptr, payload_len, sfTransactionType);
        if (txtype_lookup < 0)
            rollback(SBUF("Notary: Memo is invalid format. Should be an unsigned transaction."), 2);

        int64_t  lls_lookup = util_subfield(data_ptr, data_len, sfLastLedgerSequence);
        uint8_t* lls_ptr = SUB_OFFSET(lls_lookup) + data_ptr;
        uint32_t lls_len = SUB_LENGTH(lls_lookup);

        // check for expired txn
        if (lls_len != 4 || UINT32_FROM_BUF(lls_ptr) < ledger_seq() + MINIMUM_FUTURE_LEDGER)
            rollback(SBUF("Notary: Provided txn blob expires too soo (LastLedgerSeq)."), 3);

        // compute txn hash
        if (sto_sha512h(SBUF(invoice_id), data_ptr, data_len) < 0)
            rollback(SBUF("Notary: Could not compute sha512 over the submitted txn."), 5);

        invoice_id[31] = ( invoice_id[31] & 0xF0U ) + 0x0FU;

        // write blob to state
        if (state_set(data_ptr, data_len, SBUF(invoice_id)) != data_len)
            rollback(SBUF("Notary: Could not write txn to hook state."), 6);

    }

    // record the signature
    invoice_id[31] = ( invoice_id[31] & 0xF0U ) + found;
    
    if (state_set(&signer_weight, 2, SBUF(invoice_id)) != 2)
        rollback(SBUF("Notary: Could not write signature to hook state."), 7);

    // check if we have managed to achieve a quorum
    
    uint32_t total = 0;
    for (uint8_t i = 1; GUARD(8), i < 9; ++i)
    {
        invoice_id[31] = ( invoice_id[31] & 0xF0U ) + i;
        uint16_t weight = 0;
        if (state(&weight, 2, SBUF(invoice_id)) == 2)
            total += weight;
    }

    if (total < signer_quorum)
        accept(SBUF("Notary: Accepted signature/txn, waiting for other signers..."), 0);

    // execution to here means we must emit the txn then clean up
    // emit txn
    // emit event
    //
    accept(SBUF("Notary: Slot success"), 0);
    return 0;
}

