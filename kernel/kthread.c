#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

extern struct proc proc[NPROC];

void kthreadinit(struct proc *p)
{

  for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++)
  {

    // WARNING: Don't change this line!
    // get the pointer to the kernel stack of the kthread
    kt->kstack = KSTACK((int)((p - proc) * NKT + (kt - p->kthread)));
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

struct trapframe *get_kthread_trapframe(struct proc *p, struct kthread *kt)
{
  return p->base_trapframes + ((int)(kt - p->kthread));
}

// TODO: delte this after you are done with task 2.2
void allocproc_help_function(struct proc *p) {
  p->kthread->trapframe = get_kthread_trapframe(p, p->kthread);

  p->context.sp = p->kthread->kstack + PGSIZE;
}

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