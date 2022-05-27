#include <stdint.h>

extern int32_t _g           (uint32_t id, uint32_t maxiter);
extern int64_t accept       (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
extern int64_t state_set    (uint32_t read_ptr, uint32_t read_len, uint32_t kread_ptr, uint32_t kread_len);
extern int64_t trace_num    (uint32_t a, uint32_t b, uint64_t i);
#define SBUF(x) x, sizeof(x)
#define GUARD(n) _g(__LINE__, n+1)
int64_t cbak(uint32_t what)
{ 
    return 0;
}

int64_t hook(uint32_t reserved )
{

    uint8_t test[] =  "hello world!";
    uint8_t test2[] = "this is a much longer test string";
    trace_num(SBUF("location of test[]:"), test);
    uint8_t test_key[32];
    for (int i = 0; GUARD(32), i < 32; ++i)
        test_key[i] = i;

    int64_t result = state_set(SBUF(test), SBUF(test_key));
    
    trace_num(SBUF("state_set result:"), result);

    uint8_t zero_key[32];
    result = state_set(SBUF(test2), SBUF(zero_key));
    
    trace_num(SBUF("state_set result:"), result);

    accept (0,0,0); 
    _g(1,1);   // every hook needs to import guard function and use it at least once
    // unreachable
    return 0;
}
