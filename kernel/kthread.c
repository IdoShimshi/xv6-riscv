#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

extern struct proc proc[NPROC];

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock kthread_wait_lock;
struct trapframe *get_kthread_trapframe(struct proc *p, struct kthread *kt)
{
  return p->base_trapframes + ((int)(kt - p->kthread));
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  
  release(&mykthread()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

void kthreadinit(struct proc *p)
{
  initlock(&p->counterLock, "nexttid");
  
  for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++)
  {
    // WARNING: Don't change this line!
    // get the pointer to the kernel stack of the kthread
    kt->kstack = KSTACK((int)((p - proc) * NKT + (kt - p->kthread)));

    initlock(&kt->lock, "thread");
    kt->state = T_UNUSED;
    kt->parent = p;
  }
}

struct kthread *mykthread()
{
  push_off();
  struct cpu *c = mycpu();
  struct kthread *t = c->kt;
  pop_off();
  return t;
}

int alloctid(struct proc *p)
{
  int tid;
  
  acquire(&p->counterLock);
  tid = p->threadsCounter;
  p->threadsCounter++;
  release(&p->counterLock);

  return tid;
}


struct kthread* allocthread(struct proc *p){
  struct kthread *kt;

  for(kt = p->kthread; kt < &p->kthread[NKT]; kt++) {
    acquire(&kt->lock);
    if(kt->state == T_UNUSED) {
      goto found;
    } else {
      release(&kt->lock);
    }
  }
  return 0;

found:
  kt->tid = alloctid(p);
  kt->state = T_USED;
  kt->trapframe = get_kthread_trapframe(p, kt);
  memset(&kt->context, 0, sizeof(kt->context));
  kt->context.ra = (uint64)forkret;
  kt->context.sp = kt->kstack + PGSIZE;

  return kt;
}

void freekthread(struct kthread* k){
  if (k == 0)
      return;
      
  acquire(&k->lock);
  k->trapframe = 0;
  k->state = T_UNUSED;
  k->chan = 0;
  k->killed = 0;
  k->xstate = 0;
  k->tid = 0;
  release(&k->lock);
}

// find UNUSED thread under the calling process
// init some of this thread fields
// return tid or -1 if no UNUSED thread found
int kthread_create(void *(*start_func)(), void *stack, uint stack_size){
if (!stack || stack_size > STACK_SIZE)
  return -1;
struct proc* p = myproc();
struct kthread *kt = allocthread(p);
// no thread was initialized
if(kt == 0)
  return -1;

// if we here, let's set thread relevant fields
//set epc register for func starting point
kt->trapframe->epc = (uint64)start_func;
// " and the user-space ‘sp’ register to the top of the stack."
// 1. there's also sp register in the context of thread. consider this sp also
// 2. top of stack? hope it's stack + size
kt->trapframe->sp = (uint64)stack + stack_size;
kt->state = T_RUNNABLE;
release(&kt->lock);
return kt->tid;
}

int kthread_kill(int ktid){
  struct proc *p = myproc();
  struct kthread *kt;

  for(kt = p->kthread; kt < &p->kthread[NKT]; kt++){
    acquire(&kt->lock);
    if(kt->tid == ktid){
      kt->killed = 1;
      if(kt->state == T_SLEEPING){
      // Wake thread from sleep().
      kt->state = T_RUNNABLE;
      }
      release(&kt->lock);
      return 0;
    }
    release(&kt->lock);
  }
  return -1;
}

void kthread_exit(int status){
  struct proc *p = myproc();
  struct kthread *kt;

  int alive_threads = 0;
  for(kt = p->kthread; kt < &p->kthread[NKT]; kt++){
    acquire(&kt->lock);
    if (kt->state != T_UNUSED && kt->state != T_ZOMBIE)
      alive_threads++;
    release(&kt->lock);
  }
  if (alive_threads <= 1){
    exit(status);
  }
  kt = mykthread();
  
  acquire(&kthread_wait_lock);
  wakeup(kt);
  
  acquire(&kt->lock);
  kt->xstate = status;
  kt->state = T_ZOMBIE;
  release(&kt->lock);
  release(&kthread_wait_lock);

  acquire(&kt->lock);
  
  // Jump into the scheduler, never to return.
  sched();
  panic("thread zombie exit");
}

int kthread_join(int ktid, int *status){
  struct proc *p = myproc();
  struct kthread *kt;
  acquire(&kthread_wait_lock);
  for(kt = p->kthread; kt < &p->kthread[NKT]; kt++){
    if(kt->tid == ktid){
      for(;;){
        if (kt->state == T_ZOMBIE){
          acquire(&kt->lock);

          if(status != 0 && copyout(kt->parent->pagetable, (uint64) status, (char *)&kt->xstate,
                                      sizeof(kt->xstate)) < 0) {
            release(&kt->lock);
            release(&kthread_wait_lock);
            return -1;
          }
          release(&kt->lock);
          freekthread(kt);
          release(&kthread_wait_lock);
          return 0;
        }
        
        sleep(kt,&kthread_wait_lock);
      }
    }
  }
  release(&kthread_wait_lock);
  return -1;
}

int
kthread_killed(struct kthread *kt)
{
  int k;

  acquire(&kt->lock);
  k = kt->killed;
  release(&kt->lock);
  return k;
}