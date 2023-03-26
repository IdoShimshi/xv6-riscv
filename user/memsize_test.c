#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int msize = memsize();
    void* p;
    printf("before allocation: %d\n",msize);

    p = malloc(20000);
    msize = memsize();
    printf("after allocation: %d\n",msize);

    free(p);
    msize = memsize();
    printf("after free: %d\n",msize);

    exit(0,0);
}
