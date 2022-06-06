/**
 * This hook just accepts any transaction coming through it
 */
#include <stdint.h>

extern int32_t _g       (uint32_t id, uint32_t maxiter);
extern int64_t trace_num(uint32_t, uint32_t, int64_t);
extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);

int64_t cbak(uint32_t what)
{ 
    return 0;
}

int64_t hook(uint32_t reserved )
{
    for (int i = 0; i < 5; ++i)
    {
        _g(1,60);   // every hook needs to import guard function and use it at least once
        int c = i * 2;
        while(c--)
        {
            trace_num("hi", 2, c);
            _g(2, 60); 
        }
        
    }
    accept (0,0,0); 
    // unreachable
    return 0;
}
