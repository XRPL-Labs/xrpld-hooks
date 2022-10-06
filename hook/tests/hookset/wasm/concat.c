/**
 * This hook just accepts any transaction coming through it
 */
#include <stdint.h>

extern int32_t _g       (uint32_t id, uint32_t maxiter);
extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
extern int64_t str_concat(
        uint32_t write_ptr, uint32_t write_len,
        uint32_t read_ptr, uint32_t read_len,
        uint64_t operand, uint32_t operand_type);

extern int64_t str_find(
        uint32_t hread_ptr, uint32_t hread_len,
        uint32_t nread_ptr, uint32_t nread_len,
        uint32_t mode, uint32_t n);

#define SBUF(x) x, sizeof(x)

int64_t cbak(uint32_t what)
{ 
    return 0;
}

int64_t hook(uint32_t reserved )
{

    char x[1024];

    int64_t bytes = str_concat(SBUF(x), SBUF("testing "), 5, 1);
    accept (x, str_find(SBUF(x), 0,0,0,0), 1); 
    _g(1,1);   // every hook needs to import guard function and use it at least once
    // unreachable
    return 0;
}
