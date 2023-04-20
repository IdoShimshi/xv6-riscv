#include "user/uthread.h"
#include "user/user.h"

struct uthread uthreadList[MAX_UTHREADS];
struct uthread *curThread;

int nexttid = 1;
long long globalRound = 0;

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

void uthread_yield(){
  struct uthread *t;
  struct uthread *oldThread = uthread_self();
  struct uthread *bestThread = uthreadList;
  int bestPriority = -1;
  int bestThreadPid = 0;
  long long bestRound = 0;
  int livingProcessCount = 0;
  globalRound++;
  oldThread->state = RUNNABLE;

  if(bestThread->state == RUNNABLE){
    bestThreadPid = bestThread->tid;
    bestRound = bestThread->mylastRound;
    bestPriority = bestThread->priority;
  }
  // policy= pick the best thread to run
  for(t = uthreadList; t < &uthreadList[MAX_UTHREADS]; t++) {
    // count living threads
    if(t->state != FREE)
      livingProcessCount++;
    //
    if(t->state == RUNNABLE && t != oldThread) {
      //take better priority first
      if(t->priority > bestPriority){
        bestThread = t;
        bestThreadPid = t->tid;
        bestRound = t->mylastRound;
        bestPriority = t->priority;
      }
      // take same priority if it suspend for longer time
      else if(t->priority == bestPriority){
        if(bestRound > t->mylastRound){
          bestThread = t;
          bestThreadPid = t->tid;
          bestRound = t->mylastRound;
          bestPriority = t->priority;
        }
      }
    }
  }

  if(livingProcessCount == 1){
    bestThread = oldThread;
  }

  bestThread->mylastRound = globalRound;
  bestThread->state = RUNNING;
  curThread = bestThread;
  uswtch(&oldThread->context, &bestThread->context);

}

struct uthread* uthread_self(){
  return curThread;
}

// set new priority to cur_thread and return the old one
enum sched_priority uthread_set_priority(enum sched_priority priority){
  struct uthread *t = curThread;
  enum sched_priority old_p = t->priority;
  t->priority = priority;
  return old_p;
}

enum sched_priority uthread_get_priority(){
  return curThread->priority;
}; 

