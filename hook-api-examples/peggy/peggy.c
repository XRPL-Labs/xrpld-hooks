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

    // fake oracle
    uint8_t oracle_lo[20] = {
        0x2dU,0xd8U,0xaaU,0xdbU,0x4eU,0x15U,
        0xebU,0xeaU,0xeU,0xfdU,0x78U,0xd1U,0xb0U,\
        0x35U,0x91U,0x4U,0x7bU,0xfaU,0x1eU,0xeU};

    uint8_t oracle_hi[20] = {
        0xb5U,0xc6U,0x32U,0xd4U,0x7cU,0x3dU,
        0x37U,0x24U,0x20U,0xabU,0x3dU,0x31U,0x50U,\
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
    
    int64_t slot_user_trustline = slot_set(SBUF(keylet), 0);
    TRACEVAR(slot_user_trustline);
    if (slot_user_trustline < 0)
        rollback(SBUF("Peggy: You must have a trustline set for USD to this account."), 10);


    int compare_result = 0;
    ACCOUNT_COMPARE(compare_result, hook_accid, account_field);
    if (compare_result == 0)
        rollback(SBUF("Peggy: Invalid trustline set hi=lo?"), 1);

    int64_t lim_slot = slot_subfield(slot_user_trustline, (compare_result > 1 ? sfLowLimit : sfHighLimit), 0); 
    if (lim_slot < 0)
        rollback(SBUF("Peggy: Could not find sfLowLimit on oracle trustline"), 20);

    int64_t user_trustline_limit = slot_float(lim_slot);
    if (user_trustline_limit < 0)
        rollback(SBUF("Peggy: Could not parse user trustline limit"), 1);

    trace(SBUF("user_trustline_limit:"), 0);
    trace_float(user_trustline_limit);

    int64_t required_limit = float_set(10, 1);

    trace(SBUF("required_limit:"), 0);
    trace_float(required_limit);

    if (float_compare(user_trustline_limit, required_limit, COMPARE_EQUAL | COMPARE_GREATER) != 1)
        rollback(SBUF("Peggy: You must set a trustline for USD to peggy for limit of at least 10B"), 1);

    CLEARBUF(keylet);
    // find the oracle price value
    if (util_keylet(SBUF(keylet), KEYLET_LINE, SBUF(oracle_lo), SBUF(oracle_hi), SBUF(currency)) != 34)
        rollback(SBUF("Peggy: Internal error, could not generate keylet"), 10);

    int64_t slot_no = slot_set(SBUF(keylet), 0);
    TRACEVAR(slot_no);
    if (slot_no < 0)
        rollback(SBUF("Peggy: Could not find oracle trustline"), 10);

    lim_slot = slot_subfield(slot_no, sfLowLimit, 0);
    if (lim_slot < 0)
        rollback(SBUF("Peggy: Could not find sfLowLimit on oracle trustline"), 20);

    TRACEVAR(lim_slot);

    int64_t exchange_rate = slot_float(lim_slot);
    if (exchange_rate < 0) 
        rollback(SBUF("Peggy: Could not get exchange rate float"), 20);
    
    trace_float(exchange_rate);
   

    // process the amount sent
    int64_t oslot = otxn_slot(0);
    if (oslot < 0)
        rollback(SBUF("Peggy: Could not slot originating txn."), 1);

    int64_t amt_slot = slot_subfield(oslot, sfAmount, 0);
    if (amt_slot < 0)
        rollback(SBUF("Peggy: Could not slot otxn.sfAmount"), 2);

    int64_t amt = slot_float(amt_slot);
    if (amt < 0)
        rollback(SBUF("Peggy: Could not parse amount."), 1);

    int64_t is_xrp = slot_type(amt_slot, 1);
    if (is_xrp < 0)
        rollback(SBUF("Peggy: Could not determine sent amount type"), 3);

    if (is_xrp)
    {
        // XRP INCOMING
        int64_t pusd_amt = float_multiply(amt, exchange_rate);
        pusd_amt = float_mulratio(pusd_amt, 0, 2, 3);
        trace(SBUF("computed pusd amt: "), 0); 
        trace_float(pusd_amt);

        int64_t fee = etxn_fee_base(PREPARE_PAYMENT_SIMPLE_TRUSTLINE_SIZE);

        uint8_t amt_out[48];
        if (float_sto(SBUF(amt_out), SBUF(currency), SBUF(hook_accid), pusd_amt, 0) < 0)
            rollback(SBUF("Peggy: Could not dump pusd amount into sto"), 1);

        for (int i = 0; GUARD(20),i < 20; ++i)
        {
            amt_out[i + 28] = hook_accid[i];
            amt_out[i +  8] = currency[i];
        }

        uint8_t txn_out[PREPARE_PAYMENT_SIMPLE_TRUSTLINE_SIZE];
        PREPARE_PAYMENT_SIMPLE_TRUSTLINE(txn_out, amt_out, fee, account_field, source_tag, source_tag);

        trace(SBUF(txn_out), 1);

        if (emit(SBUF(txn_out) < 0))
            rollback(SBUF("Peggy: Emitting txn failed"), 1);

        accept(SBUF("Peggy: Sent out PUSD!"), 0);
        return 0;
    }

        // non-xrp incoming
        uint8_t amount_buffer[48];
        if (slot(SBUF(amount_buffer), amt_slot) != 48)
            rollback(SBUF("Peggy: Could not dump sfAmount"), 1);

        // ensure the issuer is us
        for (int i = 28; GUARD(20), i < 48; ++i)
        {
            if (amount_buffer[i] != hook_accid[i - 28])
                rollback(SBUF("Peggy: A currency we didn't issue was sent to us."), 1);
        }

        // ensure the currency is PUSD
        for (int i = 8; GUARD(20), i < 28; ++i)
        {
            if (amount_buffer[i] != currency[i - 8])
                rollback(SBUF("Peggy: A non USD currency was sent to us."), 1);
        }

        // execution to here means it was valid PUSD
        int64_t xrp_amt = float_divide(amt, exchange_rate);
        trace(SBUF("computed xrp amt: "), 0); 
        trace_float(xrp_amt);

/*
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

*/

    return 0;
}
