#include "hookapi.h"
int64_t cbak(int64_t reserved) { return 0; }

int64_t hook(int64_t reserved ) {
   
    GUARD(1);
    
    for (int i = 0; GUARD(10), i < 10; ++i)
    {
        for (uint32_t j = 0; GUARD(200), j < 20; ++j)
        {
            trace(&j, 4, 1);
        }
    }
  
    accept (0,0,0); 
    
    // unreachable

    return 0;

}
