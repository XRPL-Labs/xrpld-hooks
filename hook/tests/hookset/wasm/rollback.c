/**
 * This hook just accepts any transaction coming through it
 */
#include <stdint.h>

extern int32_t _g       (uint32_t id, uint32_t maxiter);
extern int64_t rollback   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);

int64_t hook(uint32_t reserved )
{
    rollback (0,0,0); 
    _g(1,1);   // every hook needs to import guard function and use it at least once
    // unreachable
    return 0;
}
