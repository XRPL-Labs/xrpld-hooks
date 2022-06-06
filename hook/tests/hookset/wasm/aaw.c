// RC:  50 : strong execution
// RC: 100 : weak execution (collect call)
// RC: 150 : again as weak (not collect / after strong)
// RC: 200 : callback execution

#include <stdint.h>

extern int32_t _g       (uint32_t id, uint32_t maxiter);
extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
extern int64_t hook_again (void);
extern int64_t trace( uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
extern int64_t trace_num (uint32_t, uint32_t, uint64_t);
extern int64_t meta_slot(uint32_t);
extern int64_t slot(uint32_t, uint32_t, uint32_t);
#define SBUF(x) (uint32_t)((void*)x), sizeof(x)
int64_t cbak(uint32_t what)
{ 
    accept (SBUF("Callback execution"),200);
}

int64_t hook(uint32_t reserved )
{
    if (reserved == 0)
    {
        hook_again();
        
        accept (SBUF("Strong execution"),50); 
    }
    

    int64_t result = 
        meta_slot(1);

    trace_num(SBUF("meta_slot(1): "), result);


    uint8_t dump[1024];
    result = slot(SBUF(dump), 1);
    trace_num(SBUF("slot(1): "), result);

    trace(SBUF("dumped txmeta:"), dump, result, 1);

    if (reserved == 1)
        accept(SBUF("Weak execution (COLLECT)"), 100);
    else
        accept(SBUF("AAW execution"), 150);


    _g(1,1);   // every hook needs to import guard function and use it at least once
    // unreachable
    return 0;
}
