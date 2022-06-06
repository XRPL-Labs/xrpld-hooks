#include <stdint.h>

extern int32_t volatile _g       (uint32_t id, uint32_t maxiter);
extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
extern int64_t trace_num(uint32_t, uint32_t, int64_t);
int64_t hook(uint32_t r)
{
    const int x = r;
    for (int i = 0; trace_num(0,0,0), _g(11, 12), i < 5; ++i)
    {
        trace_num("hi", 2, i);
    }

    accept(0,0,0);
    return 0;
}
