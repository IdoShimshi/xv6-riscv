#include "user/uthread.h"
#include "user/user.h"

struct uthread uthreadList[MAX_UTHREADS];
struct uthread *curThread;

int creationStatus = 0;
int nexttid = 1;
long long globalRound = 0;


void thread_start(void (*start_func)()) {
  start_func();
  uthread_exit();
}

struct uthread* findByPolicy(){
  struct uthread *t;
  struct uthread *oldThread = uthread_self();
  struct uthread *bestThread = uthreadList;
  int bestPriority = -1;
  long long bestRound = 0;
  int livingProcessCount = 0;
  globalRound++;
  oldThread->state = RUNNABLE;

  if(bestThread->state == RUNNABLE){
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
        bestRound = t->mylastRound;
        bestPriority = t->priority;
      }
      // take same priority if it suspend for longer time
      else if(t->priority == bestPriority){
        if(bestRound > t->mylastRound){
          bestThread = t;
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
    printf("policy just picked %d (state %d)as best-first thread to run\n",bestThread->tid,bestThread->state);

  bestThread->state = RUNNING;
  curThread = bestThread;

  return bestThread;
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
  printf("create 1\n");
  if(creationStatus==0){
    uthreadinit();
    creationStatus = 1;
  }
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
    printf("starting all\n");
    static int first = 1;
    
    if (first) {
        struct uthread *t = findByPolicy();
        printf("before first exec\n\n");
        uswtch(&t->context,&t->context);
        void (*func_ptr)(void) = (void (*)(void)) t->context.ra;
        func_ptr();
        printf("after First exec his func\n");
        return 0;
    }
    return -1;
}

void uthread_yield(){
  printf("Starting yield\n");
  struct uthread *oldThread = uthread_self();
  struct uthread *bestThread = findByPolicy();
  printf("Before swtch: old one is %d best one is %d\n",oldThread->tid,bestThread->tid);
  uswtch(&bestThread->context, &oldThread->context);
  printf("After: FUCK DA SYSTEM, old context%d \n", oldThread->context);
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

