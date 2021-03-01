/**
 * Peggy.c - An oracle based stable coin hook
 *
 * Author: Richard Holland
 * Date: 1 Mar 2021
 *
 **/

#include <stdint.h>
#include "../hookapi.h"

int64_t cbak(int64_t reserved)
{
    accept(0,0,0);
    return 0;
}

int64_t hook(int64_t reserved)
{

    etxn_reserve(1);

//r.decodeAccountID('rXUMMaPpZqPutoRszR29jtC8amWq3APkx').forEach((x)=>{process.stdout.write('0x'+x.toString(16)+',')})
//    uint8_t oracle_accid[20] = {0x5U,0xb5U,0xf4U,0x3aU,0xf7U,0x17U,0xb8U,0x19U,0x48U,0x49U,0x1fU,0xb7U,0x7U,\
//        0x9eU,0x4fU,0x17U,0x3fU,0x4eU,0xceU,0xb3U};
//
    // fake oracle
    uint8_t oracle_lo[20] = {0x2dU,0xd8U,0xaaU,0xdbU,0x4eU,0x15U,0xebU,0xeaU,0xeU,0xfdU,0x78U,0xd1U,0xb0U,\
        0x35U,0x91U,0x4U,0x7bU,0xfaU,0x1eU,0xeU};
    uint8_t oracle_hi[20] = {0xb5U,0xc6U,0x32U,0xd4U,0x7cU,0x3dU,0x37U,0x24U,0x20U,0xabU,0x3dU,0x31U,0x50U,\
        0xdcU,0x5U,0x51U,0xceU,0x9cU,0x5aU,0xf3U};

    uint8_t currency[20] = {0,0,0,0, 0,0,0,0, 0,0,0,0, 'U', 'S', 'D', 0,0,0,0,0};

    // this api fetches the AccountID of the account the hook currently executing is installed on
    // since hooks can be triggered by both incoming and ougoing transactions this is important to know
    unsigned char hook_accid[20];
    hook_account((uint32_t)hook_accid, 20);

    // next fetch the sfAccount field from the originating transaction
    uint8_t account_field[20];
    int32_t account_field_len = otxn_field(SBUF(account_field), sfAccount);
    if (account_field_len < 20)                                   // negative values indicate errors from every api
        rollback(SBUF("Peggy: sfAccount field missing!!!"), 10); // this code could never be hit in prod
                                                                  // but it's here for completeness

    // compare the "From Account" (sfAccount) on the transaction with the account the hook is running on
    int equal = 0; BUFFER_EQUAL(equal, hook_accid, account_field, 20);
    if (equal)
        accept(SBUF("Peggy: Outgoing transaction"), 20);



    uint8_t keylet[34];
    CLEARBUF(keylet);
    if (util_keylet(SBUF(keylet), KEYLET_LINE, SBUF(oracle_lo), SBUF(oracle_hi), SBUF(currency)) != 34)
        rollback(SBUF("Peggy: Internal error, could not generate keylet"), 10);

    int64_t slot_no = slot_set(SBUF(keylet), 0);
    TRACEVAR(slot_no);
    if (slot_no < 0)
        rollback(SBUF("Peggy: Could not set keylet in slot"), 10);

    int64_t result = slot_subfield(slot_no, sfLowLimit, 0);
    if (result < 0)
        rollback(SBUF("Peggy: Could not find sfLowLimit on hook account"), 20);


    uint8_t lowlim[1024];
    result = slot(SBUF(lowlim), result);
    if (result < 0) 
        rollback(SBUF("Peggy: Could not dump slot"), 20);

    int16_t exponent = 
       (((((uint16_t)lowlim[0]) << 8) + lowlim[1]) >> 6U) & 0xFF;
    exponent -= 97;

    int64_t mantissa = 
        (((int64_t)lowlim[1]) & 0b00111111U) << 48U;
        mantissa += ((int64_t)lowlim[2]) << 40U; 
        mantissa += ((int64_t)lowlim[3]) << 32U; 
        mantissa += ((int64_t)lowlim[4]) << 24U; 
        mantissa += ((int64_t)lowlim[5]) << 16U; 
        mantissa += ((int64_t)lowlim[6]) << 8U; 
        mantissa += ((int64_t)lowlim[7]); 
    

    // normalize the mantissa by multiple divisions by 1 million
    for (int i = 0; GUARD(16), i < 32 && mantissa > 1000000 && mantissa % 1000000 == 0; ++i)
    {
        mantissa /= 1000000;
        exponent += 6;
    }

    // check the amount of XRP sent with this transaction
    uint8_t amount_buffer[8];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    // if it's negative then it's a non-XRP amount, or alternatively if the MSB is set
    if (amount_len != 8 || amount_buffer[0] & 0x80) /* non-xrp amount */
        rollback(SBUF("Peggy: Non-xrp balance sent. Can't proceed!"), 30);

    // this is the actual value of the amount of XRP sent to the hook in this transaction
    int64_t amount_sent = AMOUNT_TO_DROPS(amount_buffer);

    // perform the computation for number of PUSD
    mantissa *= amount_sent * 66U;
    exponent += -8; // -6 for the drops, -2 for the 66%

    // mantissa and exponent are now ready to go into an outgoing PUSD amount

    TRACEVAR(exponent);
    TRACEVAR(mantissa);




    return 0;
}
