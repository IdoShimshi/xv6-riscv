#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/uthread.h"


void printer(void){
    printf("Hello its YeildoMaster\n");
    uthread_yield();
    printf("finish\n ");
    uthread_exit();
}

int
main(int argc, char *argv[])
{
  uthread_create(printer,LOW);
  uthread_create(printer,LOW);
  uthread_create(printer,LOW);

  uthread_start_all();
  exit(0);
}
