#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

extern struct proc proc[NPROC];

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
  release(&myproc()->lock);

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



// TODO: delte this after you are done with task 2.2
// void allocproc_help_function(struct proc *p) {
//   p->kthread->trapframe = get_kthread_trapframe(p, p->kthread);

//   p->context.sp = p->kthread->kstack + PGSIZE;
// }

void freekthread(struct kthread* k){
  if (k == 0)
      return;
      
  acquire(&k->lock);
  if(k->trapframe)
  // kfree is the same func that clean trapframe in freePROC
  // hope it'll work for cleaning thread trapframme
    kfree((void*)k->trapframe);
  k->trapframe = 0;
  k->state = T_UNUSED;
  k->chan = 0;
  k->killed = 0;
  k->xstate = 0;
  k->parent = 0;
  k->trapframe = 0; 
  release(&k->lock);
}