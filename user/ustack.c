#include "ustack.h"
#include "user/user.h"

static Header *top;

void* ustack_malloc(uint len){
    if (len > 512)
        return (void*)-1;
    
    Header *p;
    uint nunits;
    nunits = (len + sizeof(Header) - 1)/sizeof(Header) + 1;

    p = (Header*)sbrk(nunits * sizeof(Header));
    if(p == (Header*)-1)
        return (void*)-1;

    p->size = nunits;
    
    if (top == 0)
        p->ptr = 0;
    else
        p->ptr = top;
    top = p;

    return (void*)(p + 1);
}

int ustack_free(void){
    Header *p;
    int ret;
    if (top == 0)
        return -1;
    p = top;
    ret = (p->size - 1) * sizeof(Header);
    if (top->ptr == 0)
        top = 0;
    else
        top = p->ptr;

    if (sbrk(-1 * (p->size * sizeof(Header))) == (char*)-1)
        return -1;
    return ret;
}