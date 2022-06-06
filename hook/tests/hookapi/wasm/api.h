#include <stdint.h>
#include "../../../examples/hookapi.h"
#define ASSERT(x)\
{\
    if (!(x))\
        rollback(SBUF(#x), __LINE__);\
}
