#include "kernel/types.h"


typedef long Align;

union header {
    union header *ptr;
    uint size;
};

typedef union header Header;



void* ustack_malloc(uint len);

int ustack_free(void);