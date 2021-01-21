/**
 * This hook just slots any transaction coming through it
 */
#include "../hookapi.h"
#include <stdint.h>

int64_t cbak(int64_t reserved) { 
    
    accept(0,0,0);
    return 0;
}

int64_t hook(int64_t reserved ) {

    trace(SBUF("Slot.c: Called."), 0);
    
    uint8_t kl[34] = {0};
    uint8_t accid[20];
    if (hook_account(accid, 20) != 20)
        rollback(SBUF("Slot.c: could not call hook_account"), 1);

    int64_t retval = util_keylet(kl, 34, KEYLET_ACCOUNT, accid, 20, 0, 0, 0, 0);

    trace_num(SBUF("Slot.c keylet retval"), retval);
    trace(SBUF("Slot.c keylet output:"), 0);
    trace(SBUF(kl), 1);
    
    accept(0,0,0); 
    
    _g(1,1);   // every hook needs to import guard function and use it at least once
    // unreachable
    return 0;

}
