/**
 * This hook just accepts any transaction coming through it
 */
#include "../hookapi.h"

int64_t cbak(int64_t reserved) { 
    
    accept(0,0,0);
    return 0;
}

typedef struct { int8_t exponent; int64_t mantissa; } IOUAmount_t;

int64_t hook(int64_t reserved ) {

    IOUAmount_t x;
    x.exponent = 1;
    x.mantissa = 2;

    trace(SBUF("Accept.c: Called."), 0);
    accept (0,0,0); 

    

    _g(1,1);   // every hook needs to import guard function and use it at least once
    // unreachable
    return 0;

}
