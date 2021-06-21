/**
 * This hook just accepts any transaction coming through it
 */
#include <stdint.h>
#include "../hookapi.h"

int64_t cbak(uint32_t reserved) { 
    return 0;
}

int64_t hook(uint32_t reserved )
{

    uint8_t param_value[32];

    uint8_t param_name[] = { 0xDEU, 0xADU, 0xBEU, 0xEFU };
    int64_t r = hook_param(SBUF(param_value), SBUF(param_name)); 

    TRACEHEX(param_value);
    TRACEVAR(r);
    accept(0,0,0);
    _g(1,1);   // every hook needs to import guard function and use it at least once
    // unreachable
    return 0;
}
