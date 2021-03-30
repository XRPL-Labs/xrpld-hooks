/**
 * This hook just accepts any transaction coming through it
 */
#include "../hookapi.h"

int64_t cbak(int64_t reserved) { 
    
    accept(0,0,0);
    return 0;
}

int64_t hook(int64_t reserved ) {


    // create a float which is the smallest number representable by rippled
    int64_t y = float_set(-81, 1);
    
    // print it
    TRACEXFL(y);

    //multiply it by 2
    y = float_mulratio(y, 1 /* round up */, 2 /* numerator */, 1 /* denominator */);

    // print it again
    TRACEXFL(y);


    // serialize the float to an sfAmount STO and print it to log
    uint8_t float_out[9];
    CLEARBUF(float_out);
    int64_t result = float_sto(SBUF(float_out), y, sfAmount);
    if (result < 0)
        rollback(SBUF("float_sto failed"), 1);
    trace(SBUF(float_out), 1);

    trace(SBUF("Float.c: Called."), 0);
    accept (0,0,0); 

    

    _g(1,1);   // every hook needs to import guard function and use it at least once
    // unreachable
    return 0;

}
