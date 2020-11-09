#include "hookapi.h"

int64_t cbak(int64_t reserved) { 
    
    accept(0,0,0);
    return 0;
}

int64_t hook(int64_t reserved ) {
  
    accept (0,0,0); 
    
    _g(2,2);   
    // unreachable

    return 0;

}
