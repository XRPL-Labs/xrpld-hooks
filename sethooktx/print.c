#include <stdint.h>
#include "hookapi.h"

int64_t hook(int64_t reserved ) {

    unsigned char acc_buffer[20];
    char output_buffer[68];

    get_hook_account(acc_buffer, 20);           // <--- hook api call
    
    memcpy(output_buffer, "account: ", 9);
    TO_HEX(output_buffer + 9, acc_buffer, 20);  // <--- hook macro
    output_buffer[49] = '\0';
    
    output_dbg(output_buffer, 49);              // <--- hook api

    acc_bufferept( 0, 0, 0 );
    return 0;

}
