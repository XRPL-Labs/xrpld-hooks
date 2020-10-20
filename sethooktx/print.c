#include <stdint.h>
#include "hookapi.h"

int64_t hook(int64_t reserved) __attribute__((used));

int64_t cbak(int64_t reserved) 
{
    return 0;
}
int64_t hook(int64_t reserved ) {

    unsigned char output_buffer[PREPARE_PAYMENT_SIMPLE_SIZE*2];

    set_emit_count(1);

    unsigned char tx[PREPARE_PAYMENT_SIMPLE_SIZE];
//#define PREPARE_PAYMENT_SIMPLE(buf_out_master, drops_amount, drops_fee, to_address, dest_tag, src_tag)\

    unsigned char to[] = 
    {
        0xCA, 0xFE, 0xBA, 0xBE,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    }; //rKWLeCTRrocvXdgZEhP3uS8Q4keUJgoYG5


    int64_t fee_base = get_emit_fee_base(PREPARE_PAYMENT_SIMPLE_SIZE);
    DEC_TO_BUF(fee_base, output_buffer, 10);
    output_dbg(output_buffer, 10);

    PREPARE_PAYMENT_SIMPLE(tx, 1234500000ULL, fee_base,  to, 111, 22);

    TO_HEX(output_buffer, tx, PREPARE_PAYMENT_SIMPLE_SIZE);
    output_dbg(output_buffer, PREPARE_PAYMENT_SIMPLE_SIZE*2);
    
    int x = PREPARE_PAYMENT_SIMPLE_SIZE;
    emit_txn(tx, x);

    accept(0,0,0); 
    return 0;

}
