#include <stdint.h>
#include "hookapi.h"

int64_t hook(int64_t reserved ) {

//    unsigned char acc_buffer[20];
    unsigned char output_buffer[PREPARE_PAYMENT_SIMPLE_SIZE*2];
    /*
    get_hook_account(acc_buffer, 20);           // <--- hook api call
    
    memcpy(output_buffer, "account: ", 9);
    TO_HEX(output_buffer + 9, acc_buffer, 20);  // <--- hook macro
    output_buffer[49] = '\0';
    
    output_dbg(output_buffer, 49);              // <--- hook api

  */ 
    unsigned char tx[PREPARE_PAYMENT_SIMPLE_SIZE];
//#define PREPARE_PAYMENT_SIMPLE(buf_out_master, drops_amount, drops_fee, to_address, dest_tag, src_tag)\

    unsigned char to[] = { 0xB5, 0xF7, 0x62, 0x79, 0x8A, 0x53, 0xD5, 0x43, 0xA0, 0x14, 0xCA, 0xF8, 0xB2, 0x97, 0xCF, 0xF8, 0xDE, 0xAD, 0xBE, 0xEF };
    PREPARE_PAYMENT_SIMPLE(tx, 12345ULL, 678ULL,  to, 111, 22);

    TO_HEX(output_buffer, tx, PREPARE_PAYMENT_SIMPLE_SIZE);

    output_dbg(output_buffer, PREPARE_PAYMENT_SIMPLE_SIZE*2);
    
    int x = PREPARE_PAYMENT_SIMPLE_SIZE;
    emit_txn(output_buffer, x);

    accept( 0, 0, 0 );
    return 0;

}
