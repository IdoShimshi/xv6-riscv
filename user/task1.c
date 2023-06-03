#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "ustack.h"

int
main(int argc, char *argv[])
{
  for (int i = 0; i < 20; i++)
  {
     ustack_malloc(512- 16);
  }
  for (int i = 0; i < 20; i++)
  {
     printf("freed output: %d\n",ustack_free());
  }
  ustack_malloc(512- 16);
  printf("freed output: %d\n",ustack_free());

  printf("freed output: %d\n",ustack_free());

  ustack_malloc(512- 16);
  printf("freed output: %d\n",ustack_free());
  exit(0);
}
