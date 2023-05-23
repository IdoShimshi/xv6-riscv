#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    char buffer[8];
    printf("task2 test started!\n");
    getpid();
    int n = 0;
    void *p;
    while (n != -1){
        printf("choose num of bytes to alloc: ");
        if (read(0, buffer, 8) < 0)
            exit(-1);
        n = atoi(buffer);
        p = malloc(n);
        getpid();
        printf("freeing %d bytes back", n);
        free(p);
        getpid();
    }

  exit(0);
}
