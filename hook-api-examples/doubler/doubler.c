//Authors: NeilH, RichardAH
// (joke) test hook that doubles incoming XRP payments and sends it back
// April 1st 2021: Added (unfair) coin flip

#include <stdint.h>
#include "../hookapi.h"
int64_t cbak(uint32_t reserved)
{
    return 0;
}

int64_t hook(uint32_t reserved)
{
    
    uint8_t hook_accid[20];
    if (hook_account(SBUF(hook_accid)) < 0)
        rollback(SBUF("Doubler: Could not fetch hook account id."), 1);

    // next fetch the sfAccount field from the originating transaction
    uint8_t account_field[20];
    int32_t account_field_len = otxn_field(SBUF(account_field), sfAccount);

    // compare the "From Account" (sfAccount) on the transaction with the account the hook is running on
    int equal = 0; BUFFER_EQUAL(equal, hook_accid, account_field, 20);
    if (equal)
    {
        accept(SBUF("Doubler: Outgoing transaction. Passing."), 2);
        return 0;
    }

    uint8_t digest[96];
    if (ledger_last_hash(digest, 32) != 32)
        rollback(SBUF("Doubler: Failed to fetch last closed ledger."), 3);

    uint8_t key[32]; // left as 0...0
    state(digest + 32, 32, SBUF(key)); // if this load fails then we don't care, the hash is just 0
    nonce(digest + 64, 32); // todo: if we enforce sfFirstLedgerSequence = +1 then this will be impossible to cheat

    uint8_t hash[32];
    if (util_sha512h(SBUF(hash), SBUF(digest)) != 32)
        rollback(SBUF("Doubler: Could not compute digest for coin flip."), 4);

    if (state_set(SBUF(hash), SBUF(key)) != 32)
        rollback(SBUF("Doubler: Could not set state."), 5);

    // first digit of lcl hash is our biased coin flip, you lose 60% of the time :P
    if (hash[0] % 10 < 6)
        accept(SBUF("Doubler: Tails, you lose. Om nom nom xrp."), 4);

    // before we start calling hook-api functions we should tell the hook how many tx we intend to create
    etxn_reserve(1); // we are going to emit 1 transaction

    // fetch the sent Amount
    // Amounts can be 384 bits or 64 bits. If the Amount is an XRP value it will be 64 bits.
    unsigned char amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    int64_t drops_to_send = AMOUNT_TO_DROPS(amount_buffer) * 2; // doubler pays back 2x received

    if (amount_len != 8)
        rollback(SBUF("Doubler: Rejecting incoming non-XRP transaction"), 5);

    int64_t fee_base = etxn_fee_base(PREPARE_PAYMENT_SIMPLE_SIZE);
    uint8_t tx[PREPARE_PAYMENT_SIMPLE_SIZE];

    // we will use an XRP payment macro, this will populate the buffer with a serialized binary transaction
    // Parameter list: ( buf_out, drops_amount, drops_fee, to_address, dest_tag, src_tag )
    PREPARE_PAYMENT_SIMPLE(tx, drops_to_send, fee_base, account_field, 0, 0);

    // emit the transaction
    uint8_t emithash[32];
    emit(SBUF(emithash), SBUF(tx));

    // accept and allow the original transaction through
    accept(SBUF("Doubler: Heads, you won! Funds emitted!"), 0);
    return 0;

}
