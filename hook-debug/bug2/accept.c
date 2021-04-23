/**
 * This hook just accepts any transaction coming through it
 */
#include "../hookapi.h"

int64_t cbak(uint32_t reserved) { 
    trace_num(SBUF("reserved on callback"), reserved);
    uint8_t txnid[32];
    otxn_id(SBUF(txnid), 0);
    trace(SBUF("emit txnid 0:"), SBUF(txnid), 1);
    otxn_id(SBUF(txnid), 1);
    trace(SBUF("emit txnid 1:"), SBUF(txnid), 1);
    return 0;
}

int64_t hook(uint32_t reserved ) {

    TRACESTR("Bug2");

    etxn_reserve(1);

    int64_t fee_base = etxn_fee_base(PREPARE_PAYMENT_SIMPLE_SIZE);
    uint8_t tx[PREPARE_PAYMENT_SIMPLE_SIZE];

    // Parameter list: ( buf_out, drops_amount, drops_fee, to_address, dest_tag, src_tag )
    int64_t drops_to_send = 1;
    uint8_t account_field[20];
    otxn_field(SBUF(account_field), sfAccount);
    PREPARE_PAYMENT_SIMPLE(tx, drops_to_send, fee_base, account_field, 0, 0);

    // emit the transaction
    uint8_t txid[32];
    emit(SBUF(txid), SBUF(tx));

    accept (0,0,0); 

    _g(1,1);   // every hook needs to import guard function and use it at least once
    // unreachable
    return 0;
}
