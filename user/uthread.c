#include "user/uthread.h"
#include "user/user.h"

struct uthread uthreadList[MAX_UTHREADS];
struct uthread *curThread;

int nexttid = 1;

void thread_start(void (*start_func)()) {
  start_func();
  uthread_exit();
}

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

    t->context.sp = (uint64) t->ustack + STACK_SIZE; // set stack pointer to top of stack
    t->context.ra = (uint64) thread_start; // set return address to starting function

    *((void (**)(void)) (t->context.sp - 8)) = start_func;

    t->state = RUNNABLE;
    return 0;
}
// when creating a thread. i now want to free the used stack once the thread exits, how do i do that?
void uthread_exit(){
    struct uthread *myThread = uthread_self();
    myThread->state = FREE;

    uthread_yield();
}

int uthread_start_all(){
    static int first = 1;
    
    if (first) {
        struct uthread *t;
        first = 0;
        for(t = uthreadList; t < &uthreadList[MAX_UTHREADS]; t++) {
            if(t->state == RUNNABLE) {
                curThread = t;
                uthread_yield();
            }
        }
        return 0;
    }
    return -1;
}