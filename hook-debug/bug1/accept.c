/**
 * This hook just accepts any transaction coming through it
 */
#include "../hookapi.h"

int64_t cbak(int64_t reserved) { 
    return 0;
}

int64_t hook(int64_t reserved ) {

    TRACESTR("Accept.c: Called.");

    int64_t distribute_drops = float_set(9, 7);
    distribute_drops = float_mulratio(distribute_drops, 0, 1, 5); // 1/5th
    distribute_drops = float_int(distribute_drops, 0, 0);

    accept (0,0,0); 

    _g(1,1);   // every hook needs to import guard function and use it at least once
    // unreachable
    return 0;
}
