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
    int64_t invoice_id_len = otxn_field(SBUF(invoice_id), sfInvoiceID);
    if (invoice_id_len == 32)
    {
        // select the blob by setting the last nibble to F
        invoice_id[31] = ( invoice_id[31] & 0xF0U ) + 0x0FU;
        tx_len = state(SBUF(tx_blob), SBUF(invoice_id));
        if (tx_len < 0)
            rollback(SBUF("Notary: Received invoice id that did not correspond to a submitted multisig txn."), 1);

        // blob exists, check expiry
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
    }

    // check for the presence of a memo
    uint8_t memos[MAX_MEMO_SIZE];
    int64_t memos_len = otxn_field(SBUF(memos), sfMemos);

    uint32_t payload_len = 0;
    uint8_t* payload_ptr = 0;

    if (memos_len <= 0 && invoice_id_len <= 0)
        accept(SBUF("Notary: Incoming txn with neither memo nor invoice ID, passing."), 0);

    if (memos_len > 0 && invoice_id_len > 0)
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

    int64_t result = slot_subfield(slot_no, sfSignerQuorum, 0);
    if (result < 0)
        rollback(SBUF("Notary: Could not find sfSignerQuorum on hook account"), 20);

    uint32_t signer_quorum = 0;
    uint8_t buf[4];
    result = slot(SBUF(buf), result);
    if (result != 4)
        rollback(SBUF("Notary: Could not fetch sfSignerQuorum from sfSignerEntries."), 80);
    
    signer_quorum = UINT32_FROM_BUF(buf);
    TRACEVAR(signer_quorum);

    result = slot_subfield(slot_no, sfSignerEntries, slot_no);
    if (result < 0)
        rollback(SBUF("Notary: Could not find sfSignerEntries on hook account"), 20);

    int64_t signer_count = slot_count(slot_no);
    if (signer_count < 0)
        rollback(SBUF("Notary: Could not fetch sfSignerEntries count"), 30);



    int subslot = 0;
    uint8_t found = 0;
    uint16_t signer_weight = 0;

    for (int i = 0; GUARD(8), i < signer_count + 1; ++i)
    {
        subslot = slot_subarray(slot_no, i, subslot);
        if (subslot < 0)
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

        result = slot(buf, 2, result);

        if (result != 2)
            rollback(SBUF("Notary: Could not fetch sfSignerWeight from sfSignerEntry."), 80);

        signer_weight = UINT16_FROM_BUF(buf);

        TRACEVAR(signer_weight);
        TRACEHEX(account_field);
        TRACEHEX(signer_account);
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

    if (memos_len > 0)
    {
        if (invoice_id_len > 0)
            rollback(SBUF("Notary: Incoming transaction with both invoice id and memo. Aborting."), 0);

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
        int64_t txtype_lookup      = sto_subfield(data_ptr, data_len, sfTransactionType);
        if (txtype_lookup < 0)
            rollback(SBUF("Notary: Memo is invalid format. Should be an unsigned transaction."), 2);

        int64_t  lls_lookup = sto_subfield(data_ptr, data_len, sfLastLedgerSequence);
        uint8_t* lls_ptr = SUB_OFFSET(lls_lookup) + data_ptr;
        uint32_t lls_len = SUB_LENGTH(lls_lookup);

        // check for expired txn
        if (lls_len != 4 || UINT32_FROM_BUF(lls_ptr) < ledger_seq() + MINIMUM_FUTURE_LEDGER)
            rollback(SBUF("Notary: Provided txn blob expires too soo (LastLedgerSeq)."), 3);

        // compute txn hash
        if (util_sha512h(SBUF(invoice_id), data_ptr, data_len) < 0)
            rollback(SBUF("Notary: Could not compute sha512 over the submitted txn."), 5);

        TRACEHEX(invoice_id);

        invoice_id[31] = ( invoice_id[31] & 0xF0U ) + 0x0FU;

        // write blob to state
        if (state_set(data_ptr, data_len, SBUF(invoice_id)) != data_len)
            rollback(SBUF("Notary: Could not write txn to hook state."), 6);

    }

    // record the signature
    invoice_id[31] = ( invoice_id[31] & 0xF0U ) + found;
    
    UINT16_TO_BUF(buf, signer_weight);
    if (state_set(buf, 2, SBUF(invoice_id)) != 2)
        rollback(SBUF("Notary: Could not write signature to hook state."), 7);

    // check if we have managed to achieve a quorum
    
    uint32_t total = 0;
    for (uint8_t i = 1; GUARD(8), i < 9; ++i)
    {
        invoice_id[31] = ( invoice_id[31] & 0xF0U ) + i;
        if (state(buf, 2, SBUF(invoice_id)) == 2)
            total += UINT16_FROM_BUF(buf);
    }

    TRACEVAR(total);
    TRACEVAR(signer_quorum);
    if (total < signer_quorum)
        accept(SBUF("Notary: Accepted signature/txn, waiting for other signers..."), 0);

    // execution to here means we must emit the txn then clean up
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
    
    if (emit(buffer2, tx_len) < 0)
        accept(SBUF("Notary: All conditions met but emission failed: proposed txn was malformed."), 1);

    accept(SBUF("Notary: Emitted multisigned txn"), 0);
    return 0;
}

