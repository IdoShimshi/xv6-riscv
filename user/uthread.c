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
  struct uthread *bestThread = 0;
  int bestPriority = -1;
  long long bestRound = 0;
  // int livingProcessCount = 0;
  globalRound++;

  // if(bestThread->state == RUNNABLE){
  //   bestRound = bestThread->mylastRound;
  //   bestPriority = bestThread->priority;
  // }
  // policy= pick the best thread to run
  for(t = uthreadList; t < &uthreadList[MAX_UTHREADS]; t++) {
    printf("thread %d has state %d, prio %d and round %d\n",t->tid,t->state,t->priority,t->mylastRound);
    // count living threads
    // if(t->state != FREE)
    //   livingProcessCount++;
    //
    if(t->state == RUNNABLE) {
      //take better priority first
      if(bestThread == 0 || t->priority > bestPriority){
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

  bestThread->mylastRound = globalRound;
  printf("policy just picked %d (state %d)as best-first thread to run\n",bestThread->tid,bestThread->state);
  // curThread = bestThread;
  // bestThread->state = RUNNING;
  
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
    t->mylastRound = 0;

    t->context.sp = (uint64) t->ustack + STACK_SIZE; // set stack pointer to top of stack
    t->context.ra = (uint64) start_func; // set return address to starting function

    // *((void (**)(void)) (t->context.sp - 8)) = start_func;

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
        curThread = t;
        t->state = RUNNING;
        struct context tempC;
        uswtch(&tempC,&t->context);
        // printf("before first exec\n");
        // void (*func_ptr)(void) = (void (*)(void)) t->context.ra;
        // func_ptr();
        // printf("after First exec his func\n");
        return 0;
    }
    return -1;
}

void uthread_yield(){
  // printf("Starting yield\n");
  struct uthread *oldThread = uthread_self();
  if (oldThread->state == RUNNING)
    oldThread->state = RUNNABLE;
  struct uthread *bestThread = findByPolicy();
  if (bestThread == 0)
  {
    exit(0);
  }
  
  bestThread->state = RUNNING;
  curThread = bestThread;
  // printf("Before swtch: old one is %d best one is %d\n",oldThread->tid,bestThread->tid);
  uswtch(&oldThread->context, &bestThread->context);
  // printf("After: FUCK DA SYSTEM, old context%d \n", oldThread->context);
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

