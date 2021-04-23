/**
 * This hook just accepts any transaction coming through it
 */
#include "../hookapi.h"

int64_t cbak(int64_t reserved) { 
    trace_num(SBUF("reserved on callback"), reserved); 
    return 0;
}

int64_t hook(int64_t reserved ) {

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
    emit(SBUF(tx));

    accept (0,0,0); 

    _g(1,1);   // every hook needs to import guard function and use it at least once
    // unreachable
    return 0;
}
