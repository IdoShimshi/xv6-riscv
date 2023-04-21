#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/uthread.h"


void printer1(void){
    printf("Hello 1\n");
    uthread_yield();
    printf("World 1\n ");
    uthread_exit();
}
void printer2(void){
    printf("Hello 2\n");
    uthread_yield();
    printf("World 2\n ");
    uthread_exit();
}
void printer3(void){
    printf("Hello 3\n");
    uthread_yield();
    printf("World 3\n ");
    uthread_exit();
}

int
main(int argc, char *argv[])
{
  uthread_create(printer1,LOW);
  uthread_create(printer2,LOW);
  uthread_create(printer3,LOW);

  uthread_start_all();
  exit(0);
}
