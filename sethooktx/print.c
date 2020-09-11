#include <stdint.h>
#include "hookapi.h"

//extern int64_t get_tx_field ( uint32_t field_id, uint32_t data_ptr_out, uint32_t out_len );

int64_t hook(int64_t reserved ) {

    char buffer[129];
    buffer[0] = '\0';
    memcpy(buffer, "hook saw sfDestination was ", 27);
    if (get_tx_field(sfDestination, buffer + 27, 101) <= 0)
        reject(1, 0, 0);

    output_dbg(buffer, 128);

    int out = 0;
    BUF_TO_DEC("98765", 5, out);
    DEC_TO_BUF(out, buffer, 128);
    output_dbg(buffer, 128);

    

    accept( 0, 0, 0 );
    return 0;

}
