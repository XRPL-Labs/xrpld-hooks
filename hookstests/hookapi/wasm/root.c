#include "api.h"

int64_t hook(uint32_t reserved )
{
    int64_t x = float_set(0, 1234567890);
    int64_t l = float_root(x, 2);
    TRACEXFL(l);
    int64_t i = float_int(l, 6, 1);

    TRACEVAR(i);

    ASSERT(i == 35136418286444ULL);


    // RH TODO: more tests

    accept (0,0,0); 
    _g(1,1);   // every hook needs to import guard function and use it at least once
    // unreachable
    return 0;
}
