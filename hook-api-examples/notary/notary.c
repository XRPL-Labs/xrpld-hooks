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

    uint8_t keylet[34];
    if (util_keylet(SBUF(keylet), KEYLET_SIGNERS, SBUF(account_field), 0, 0, 0, 0) != 34)
        rollback(SBUF("Notary: Internal error, could not generate keylet"), 10);

    if (slot_set(SBUF(keylet), 0) < 0)
        rollback(SBUF("Notary: Could not set keylet in slot"), 10);

    accept(SBUF("Notary: Slot success"), 0);
    return 0;
}

