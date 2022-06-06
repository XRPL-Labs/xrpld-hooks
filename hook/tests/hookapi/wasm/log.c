#include "api.h"

int64_t hook(uint32_t reserved )
{
    int64_t x = float_set(0, 1234567890);
    int64_t l = float_log(x);
    int64_t i = float_int(l, 15, 1);

    ASSERT(i == 9091514977169268ULL);

    ASSERT(float_log(float_one()) == 0);

    // RH TODO: more tests

    accept (0,0,0); 
    _g(1,1);   // every hook needs to import guard function and use it at least once
    // unreachable
    return 0;
}
