#include "ustack.h"
#include "user/user.h"

static Header *top;

void* ustack_malloc(uint len){
    printf("%d\n",sizeof(Header));
    if (len > 512)
        return (void*)-1;
    
    Header *p;
    uint nunits;
    nunits = (len + sizeof(Header) - 1)/sizeof(Header) + 1;

    p = (Header*)sbrk(nunits * sizeof(Header));
    if(p == (Header*)-1)
        return (void*)-1;

    p->s.size = nunits;
    if (top == 0)
        p->s.ptr = 0;
    else
        p->s.ptr = top;
    top = p;
    

    return (void*)(p + 1);
}

int ustack_free(void){
    Header *p;
    int ret;
    if (top == 0)
        return -1;
    p = top;
    ret = (p->s.size - 1) * sizeof(Header);
    if (top->s.ptr == 0)
        top = 0;
    else
        top = p->s.ptr;

    if (sbrk(-1 * (p->s.size * sizeof(Header))) == (char*)-1)
        return -1;
    return ret;
}