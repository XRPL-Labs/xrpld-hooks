/**
 * Peggy.c - An oracle based stable coin hook
 *
 * Author: Richard Holland
 * Date: 1 Mar 2021
 *
 **/

#define NORMALIZE(mantissa, exponent)\
{\
    if (mantissa == 0)\
        exponent = 0;\
    else\
    {\
        for (int i = 0; GUARDM(16,1), i < 16 && mantissa > 10000000000000000U; ++i)\
        {\
            mantissa /= 10;\
            exponent++;\
        }\
        for (int i = 0; GUARDM(16,2), i < 16 && mantissa < 1000000000000000U; ++i)\
        {\
            mantissa *= 10;\
            exponent--;\
        }\
    }\
    if (exponent < -96 || exponent > 80)\
        rollback(SBUF("Peggy: Internal error, invalid exponent"), 1);\
}

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

    // get the source tag if any... negative means it wasn't provided
    int64_t source_tag = otxn_field(0,0, sfSourceTag);
    if (source_tag < 0)
        source_tag = 0xFFFFFFFFU;

    // compare the "From Account" (sfAccount) on the transaction with the account the hook is running on
    int equal = 0; BUFFER_EQUAL(equal, hook_accid, account_field, 20);
    if (equal)
        accept(SBUF("Peggy: Outgoing transaction"), 20);


    uint8_t keylet[34];
    CLEARBUF(keylet);
    // check if a trustline exists between the sender and the hook for the USD currency [ PUSD ]
    if (util_keylet(SBUF(keylet), KEYLET_LINE, SBUF(hook_accid), SBUF(account_field), SBUF(currency)) != 34)
        rollback(SBUF("Peggy: Internal error, could not generate keylet"), 10);
    
    int64_t trustline_slot = slot_set(SBUF(keylet), 0);
    TRACEVAR(trustline_slot);
    if (trustline_slot < 0)
        rollback(SBUF("Peggy: You must have a trustline set for USD to this account."), 10);

    CLEARBUF(keylet);
    // find the oracle price value
    if (util_keylet(SBUF(keylet), KEYLET_LINE, SBUF(oracle_lo), SBUF(oracle_hi), SBUF(currency)) != 34)
        rollback(SBUF("Peggy: Internal error, could not generate keylet"), 10);

    int64_t slot_no = slot_set(SBUF(keylet), 0);
    TRACEVAR(slot_no);
    if (slot_no < 0)
        rollback(SBUF("Peggy: Could not find oracle trustline"), 10);


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

    uint64_t mantissa = 
        (((uint64_t)lowlim[1]) & 0b00111111U) << 48U;
        mantissa += ((uint64_t)lowlim[2]) << 40U; 
        mantissa += ((uint64_t)lowlim[3]) << 32U; 
        mantissa += ((uint64_t)lowlim[4]) << 24U; 
        mantissa += ((uint64_t)lowlim[5]) << 16U; 
        mantissa += ((uint64_t)lowlim[6]) << 8U; 
        mantissa += ((uint64_t)lowlim[7]); 
    

    // check the amount of XRP sent with this transaction
    uint8_t amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    // if it's negative then it's a non-XRP amount, or alternatively if the MSB is set
    TRACEVAR(amount_len);
    if (amount_len < 0 || (amount_len != 48 && amount_len != 8))
        rollback(SBUF("Peggy: Error reading the amount of xrp or usd sent to the hook."), 1);

    // buffer to store the soon-to-be emitted txn
    uint8_t txn_out[PREPARE_PAYMENT_SIMPLE_TRUSTLINE_SIZE];

    trace(amount_buffer, amount_len, 1);
    if (amount_len == 8)
    {
        // XRP incoming

        // this is the actual value of the amount of XRP sent to the hook in this transaction
        int64_t amount_sent = AMOUNT_TO_DROPS(amount_buffer);

        // perform the computation for number of PUSD
        uint64_t new_mantissa = 0;
        for (int i = 0; GUARD(16), i < 16; ++i)
        {
            new_mantissa = mantissa * amount_sent * 66U;
            if (new_mantissa > mantissa)
                break;
            // execution to here means we had an overflow when multiplying the mantissa

        }
        exponent -= 8; // -6 for the drops, -2 for the 66%

        // normalize for serialization purposes
        NORMALIZE(mantissa, exponent);

        // mantissa and exponent are now ready to go into an outgoing PUSD amount

        // check if their trustline limit is sufficient to receive the PUSD they are creating

        uint8_t exponent_out = (uint8_t)(exponent + 97);
        
        TRACEVAR(exponent_out);
        TRACEVAR(mantissa);

        CLEARBUF(amount_buffer);

        // set sign and non-xrp bits and first 6 bits of exponent
        amount_buffer[0] = 0b11000000U;
        amount_buffer[0] += (uint8_t)(exponent_out >> 2U);
        
        // set least significant 2 bits of exponent and first 6 bits of mantissa
        amount_buffer[1] = (exponent_out & 0b11U) << 6U;
        amount_buffer[1] += ((uint8_t)((mantissa >> 48U) & 0b111111U));
        // set the remaining mantissa bytes
        amount_buffer[2] = (uint8_t)((mantissa >> 40U) & 0xFFU);
        amount_buffer[3] = (uint8_t)((mantissa >> 32U) & 0xFFU);
        amount_buffer[4] = (uint8_t)((mantissa >> 24U) & 0xFFU);
        amount_buffer[5] = (uint8_t)((mantissa >> 16U) & 0xFFU);
        amount_buffer[6] = (uint8_t)((mantissa >>  8U) & 0xFFU);
        amount_buffer[7] = (uint8_t)((mantissa >>  0U) & 0xFFU);

        // set the currency code... since we have prefilled 0's we only need to set U S D in the right spot
        amount_buffer[20] = currency[12];
        amount_buffer[21] = currency[13];
        amount_buffer[22] = currency[14];

        // set the issuer account
        for (int i = 0; GUARD(20), i < 20; ++i)
            amount_buffer[28 + i] = hook_accid[i];

        trace(amount_buffer, 48, 1);

        int64_t fee = etxn_fee_base(PREPARE_PAYMENT_SIMPLE_TRUSTLINE_SIZE);

        /// amount_buffer ready to be placed into emitted tx
        PREPARE_PAYMENT_SIMPLE_TRUSTLINE(txn_out, amount_buffer, fee, account_field, source_tag, 0);
        emit(SBUF(txn_out));

        // check if state currently exists
        uint8_t vault[16];
        if (state(SBUF(vault), SBUF(account_field)) == 16)
        {
            // fetch serialized vault state for PUSD
            int16_t exponent_vault = ((int16_t)vault[0]) - 97;
            uint64_t mantissa_vault = ((int64_t)vault[1]) << 48U;
            mantissa_vault += ((int64_t)vault[2]) << 40U; 
            mantissa_vault += ((int64_t)vault[3]) << 32U; 
            mantissa_vault += ((int64_t)vault[4]) << 24U; 
            mantissa_vault += ((int64_t)vault[5]) << 16U; 
            mantissa_vault += ((int64_t)vault[6]) << 8U; 
            mantissa_vault += ((int64_t)vault[7]); 

            TRACEVAR(mantissa_vault);
            TRACEVAR(exponent_vault);

            // adjust floating points until they line up
            for (int i = 0; GUARD(16), i < 16 && exponent != exponent_vault; ++i)
            {
                if (exponent > exponent_vault)
                {
                    mantissa *= 10;
                    exponent--;
                } else if (exponent < exponent_vault)
                {
                    mantissa_vault *= 10;
                    exponent_vault--;
                }
            }

            // add values toghether
            mantissa += mantissa_vault;

            TRACEVAR(mantissa);
            TRACEVAR(exponent);
        
            NORMALIZE(mantissa, exponent);

            // fetch serialized xrp drops
            int64_t drops_vault = 
                (((int64_t)vault[ 8]) << 56U) +
                (((int64_t)vault[ 9]) << 48U) +
                (((int64_t)vault[10]) << 40U) +
                (((int64_t)vault[11]) << 32U) +
                (((int64_t)vault[12]) << 24U) +
                (((int64_t)vault[13]) << 16U) +
                (((int64_t)vault[14]) <<  8U) +
                (((int64_t)vault[16]) <<  0U);

            TRACEVAR(drops_vault);

            // add to amount
            amount_sent += drops_vault;
        }

        // serialize into vault
        CLEARBUF(vault);
        vault[0] = (uint8_t)(exponent + 97);
        UINT64_TO_BUF((vault+1), mantissa);
        UINT64_TO_BUF((vault+8), amount_sent);

        if (state_set(SBUF(vault), SBUF(account_field)) < 0)
            rollback(SBUF("Peggy: Internal error - Failed to set vault."), 2);

        accept(SBUF("Peggy: Emitted a PUSD txn to you."), 1);
    }
    else
    {
        // trustline incoming
        //todo: ensure currency = USD and issuer is this hook
        int64_t fee = etxn_fee_base(PREPARE_PAYMENT_SIMPLE_SIZE);
    }



    return 0;
}