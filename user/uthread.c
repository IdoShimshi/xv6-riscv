#include "user/uthread.h"
#include "user/user.h"

struct uthread uthreadList[MAX_UTHREADS];
struct uthread *curThread;

int nexttid = 1;

void
uthreadinit(void)
{
  struct uthread *t;
//   initialize locks if needed
  for(t = uthreadList; t < &uthreadList[MAX_UTHREADS]; t++) {
    //   initlock(&p->lock, "proc");
      t->state = FREE;
    //   p->kstack = KSTACK((int) (p - proc));
  }
}


int uthread_create(void (*start_func)(), enum sched_priority priority){
  struct uthread *t;

  for(t = uthreadList; t < &uthreadList[MAX_UTHREADS]; t++) {
    if(t->state == FREE) {
      goto found;
    }
  }
  return -1;

found:

    t->tid = nexttid++;
    t->priority = priority;
    char* stack = (char*) malloc(STACK_SIZE);
    if (stack == 0) {
        return -1;  // or another error code
    }
    memcpy(t->ustack, stack, STACK_SIZE);

    t->context.sp = (uint64) t->ustack + STACK_SIZE; // set stack pointer to top of stack
    t->context.ra = (uint64) start_func; // set return address to starting function
    t->state = RUNNABLE;

    
    return 0;
}