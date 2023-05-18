#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "ustack.h"

int
main(int argc, char *argv[])
{
    ustack_malloc(400);
    char* p = ustack_malloc(250);
    p = "hello";
    printf("%s\n",p);
    printf("first: %d\n",ustack_free());
    printf("second: %d\n",ustack_free());
  exit(0);
}
