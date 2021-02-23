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

    // inspect unsigned payload
    // inspect invoice id
    // check for expired txn

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

    int subslot = 0;
    int found = 0;
    for (int i = 0; GUARD(8), i < signer_count; ++i)
    {
        result = slot_subarray(slot_no, 0, subslot);
        if (result < 0)
            rollback(SBUF("Notary: Could not fetch one of the sfSigner entries [subarray]."), 40);

        result = slot_subfield(subslot, sfAccount, subslot);
        if (result < 0)
            rollback(SBUF("Notary: Could not fetch one of the account entires in sfSigner."), 50);

        uint8_t signer_account[20];
        result = slot(SBUF(signer_account), subslot);
        if (result != 20)
            rollback(SBUF("Notary: Could not fetch one of the sfSigner entries [slot sfAccount]."), 60);

        int equal = 0;
        BUFFER_EQUAL_GUARD(equal, signer_account, 20, account_field, 20, 8);

        if (equal)
        {
            found = 1;
            break;
        }
    }
    
    if (!found)
        rollback(SBUF("Notary: Your account was not present in the signer list."), 70);

    // set state accordingly
    // may need to inject blob tx
    // always need to record signer's signing action
    // always need to check if we have managed to achieve a quorum
    // emit txn
    // emit event
    //
    accept(SBUF("Notary: Slot success"), 0);
    return 0;
}

