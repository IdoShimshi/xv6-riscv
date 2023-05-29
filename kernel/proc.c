#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;


char buffer[PGSIZE];
struct spinlock buffer_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

void printBinary(uint value) {
  char binary[33];  // 32 bits + null terminator
  binary[32] = '\0';  // Null terminator

  // Convert the value to binary representation
  for (int i = 31; i >= 0; i--) {
    binary[i] = (value & 1) ? '1' : '0';
    value >>= 1;
  }

  // Print the binary representation
  printf("%s\n", binary);
}
// initialize the proc table.
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  initlock(&buffer_lock, "buffer_lock");
  
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->state = UNUSED;
      p->kstack = KSTACK((int) (p - proc));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid()
{
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;
  p->inExec = 0;
  p->pageNum = 0;
  p->pagesInRam = 0;
  p->queueCurrentSize = 0;
  p->clock_hand = 0;
  initMetadata(p);


  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable){
    proc_freepagetable(p->pagetable, p->sz);
  }
    
  p->swapFile = 0;
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
  
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy initcode's instructions
  // and data into it.
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();
  

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();
  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  createSwapFile(np);
  if (p != initproc){ // if parent is init, dont copy its file.
      copySwapFile(p, np);
  }
    

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);
  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  removeSwapFile(p);
  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          // Found one.
          pid = pp->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

void updateDatastructures(struct proc *p){
  int i=0;
  if (p->pid < 3)
    return;
  for(i = 0;i<MAX_TOTAL_PAGES;i++){
    if(p->swapMetadata[i].inFile == -1 && p->swapMetadata[i].pa != 0){
      pte_t *pt = walk(p->pagetable, p->swapMetadata[i].va, 0);
      p->swapMetadata[i].agingCounter = (p->swapMetadata[i].agingCounter >> 1);
      if( *pt & PTE_A){
        p->swapMetadata[i].agingCounter = p->swapMetadata[i].agingCounter | (1 << 31);
        *pt &= ~PTE_A;
     }
    //  if(p->swapMetadata[i].agingCounter>0){
    //   printf("%d: %x|",i,p->swapMetadata[i].agingCounter);
    //  }
    }
  }
};




// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();

    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);
        if (p->pid > 2){
          updateDatastructures(p);
        }
        
        
        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      release(&p->lock);
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
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

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}

int findSmallestFreeFileIndex(struct proc *p){
  int indexs[MAX_TOTAL_PAGES];
  memset(indexs, 0, sizeof(int) * MAX_TOTAL_PAGES);
  for (int i = 0; i < MAX_TOTAL_PAGES; i++)
  {
    if (p->swapMetadata[i].inFile != -1){
      indexs[p->swapMetadata[i].inFile] = 1;
    }
  }

  for (int i = 0; i < MAX_TOTAL_PAGES; i++)
  {
    if (indexs[i] == 0)
      return i;
  }

  return -1;
}

int countOnes(uint input) {
  int count = 0;
  while (input != 0) {
    input &= (input - 1); // Clears the rightmost set bit
    count++;
  }
  return count;
}

int pageSwapPolicy(struct proc *p){
  int lowestCounter = -1;
  int ans=0;
  int i=0;
  //SWAP_ALGO=NFUA
  if(SWAP_POLICY == NFUA){
    for(i = 0; i < MAX_TOTAL_PAGES ;i++){
      if (p->swapMetadata[i].inFile == -1 && p->swapMetadata[i].pa != 0){ //make sure page in ram
        if(p->swapMetadata[i].agingCounter < lowestCounter || lowestCounter == -1){
          lowestCounter = p->swapMetadata[i].agingCounter;
          ans = i;
        }
      }
    }
    return ans;
  }
  //SWAP_ALGO=LAPA
  // finds the page with the lowest numbers of one's at agingCounter
  if(SWAP_POLICY == LAPA){
    int j =0;
    uint lowestValue=0;
    lowestCounter = 0;
    //init for first one in ram as minimum 
    while(j<32){
        if(p->swapMetadata[j].pa!=0  && p->swapMetadata[j].inFile == -1){
          lowestCounter = countOnes(p->swapMetadata[j].agingCounter);
          ans = j;
          lowestValue = p->swapMetadata[j].agingCounter;
          break;
        }
        j++;           
    }

    if(j>31)
      printf("policy2 error: probably no index to remove\n");

    //find the real minimum 
    for(i = 0;i < MAX_TOTAL_PAGES;i++){
      if(p->swapMetadata[i].pa !=0 && p->swapMetadata[i].inFile==-1 && countOnes(p->swapMetadata[i].agingCounter) <= lowestCounter){
        if(countOnes(p->swapMetadata[i].agingCounter) == lowestCounter){
          if(p->swapMetadata[i].agingCounter < lowestValue){
            lowestCounter = countOnes(p->swapMetadata[i].agingCounter);
            ans = i;
            lowestValue = p->swapMetadata[i].agingCounter;
          }
        }
        else{
        lowestCounter = countOnes(p->swapMetadata[i].agingCounter);
        ans = i;
        lowestValue = p->swapMetadata[i].agingCounter;
        }
        
      }
    }
    return ans;
  }
  //SWAP_ALGO=SCFIFO
  // 
  if(SWAP_POLICY == SCFIFO){
    int removedIndex = removeFromQueueByPolicy(p);
    
    if(removedIndex==-1){
      printf("error in 3 Policy: index to remove failed\n");
    }
    ans = removedIndex;
    
    return ans;
  }

  printf("no policy chosen\n");
  return 0;
}

int getPageFromSwapFile(struct proc *p, uint64 va){
  void* pa;
  int index;
  int fileIndex;
  va = PGROUNDDOWN(va);

  pte_t *entry = walk(p->pagetable, va, 0);
  int perm = PTE_FLAGS(*entry);
  for (index = 0; index < MAX_TOTAL_PAGES; index++)
  {
    if (p->swapMetadata[index].va == va){
      if ((fileIndex = p->swapMetadata[index].inFile) == -1)
        return -1; // page already in ram
      pa = kalloc();
      acquire(&p->lock);
      p->swapMetadata[index].pa = (uint64) pa;
      p->swapMetadata[index].inFile = -1;
      p->swapMetadata[index].agingCounter = 0;
      p->pagesInRam++;
      //if(policy2)
      if(1)
        p->swapMetadata[index].agingCounter = 0xFFFFFFFF;
      release(&p->lock);
      if (readFromSwapFile(p, (char*)pa, fileIndex*PGSIZE, PGSIZE) < PGSIZE)
        return -1;
      if (mappages(p->pagetable, va, PGSIZE,(uint64) pa, perm) == -1)
        return -1;
      *entry &= ~PTE_PG;

      return 0;
    }
  }
  return -1;
}


int swapPageOut(struct proc *p){
  // evicts a page from proc p's pages in ram.
  int metadataIndex = pageSwapPolicy(p);
  //printMetadata(p);

  pte_t* toSwapEntry = walk(p->pagetable, p->swapMetadata[metadataIndex].va, 0);
  int fileIndex;
  if((fileIndex = findSmallestFreeFileIndex(p)) == -1){
    return -1; 
  }

   //printf("page %d being swapped out to fileIndex: %d\n", metadataIndex, fileIndex);
   //printf("%p %p %p %p \n", p, (char*) p->swapMetadata[metadataIndex].pa, fileIndex*PGSIZE, PGSIZE);
  if (writeToSwapFile(p, (char*) p->swapMetadata[metadataIndex].pa, fileIndex*PGSIZE, PGSIZE) < PGSIZE){
    return -1;
  }

  kfree((void*) p->swapMetadata[metadataIndex].pa);

  *toSwapEntry |= PTE_PG;
  *toSwapEntry &= ~PTE_V;
  
  p->swapMetadata[metadataIndex].pa = 0;
  p->swapMetadata[metadataIndex].inFile = fileIndex;
  p->pagesInRam--;
  return metadataIndex;
}

// read each page from parent->swapFile to buffer
// write buffer to p->swapFile
// copy swapMetadata from parent to p
int copySwapFile(struct proc *parent, struct proc* p) {
    // Iterate through each page in the parent's swapFile
    int lastRead = PGSIZE;
    int fileOffset = 0;
    acquire(&buffer_lock);
    while(lastRead==PGSIZE){
      // read page by page
      lastRead = readFromSwapFile(parent, buffer, fileOffset, PGSIZE);
      if(writeToSwapFile(p,buffer,fileOffset,lastRead)==-1){
        return -1;
      }
      fileOffset = fileOffset + PGSIZE;
    }
    release(&buffer_lock);
    if (lastRead < 0){
        // Error occurred while reading from swap file
        return -1;
    }
    //// deep copy parent swapMetaData
    // for (int i = 0;  p->swapMetadata !=0 && i < MAX_TOTAL_PAGES; i++) {
    //     p->swapMetadata[i] = parent->swapMetadata[i];
    // }
    // p->pageNum = parent->pageNum;
    // p->pagesInRam = parent->pagesInRam;
    return 0; // Success
}

int newPage(pagetable_t pagetable, uint64 va, uint64 pa){
  struct proc *p = myproc();
  if (p->inExec)
    goto foundptbl1;
  for(p = proc; p < &proc[NPROC]; p++) {
    if (p->pagetable == pagetable)
      goto foundptbl1;
  }
  return -1;

foundptbl1:

  if (p->pid < 3)
    return 0;
  //printf("in new page, pid %d has %d pages in ram, %d in total\n",p->pid, p->pagesInRam, p->pageNum);

  if (++(p->pageNum) > MAX_TOTAL_PAGES && p->pid > 2){
    printf("Max total pages exceeded\n");
    return -1;
  }
  if (++(p->pagesInRam) > MAX_PSYC_PAGES && p->pid > 2){
    if (swapPageOut(p) == -1){
      printf("error swapping page out\n");
      return -1;
    }
  }
  
  for (int i = 0; i < MAX_TOTAL_PAGES; i++)
  {
    if (p->swapMetadata[i].va == 0 && p->swapMetadata[i].pa == 0 &&  p->swapMetadata[i].inFile == -1){
      p->swapMetadata[i].va = va;
      p->swapMetadata[i].pa = pa;
      p->swapMetadata[i].inFile = -1;
      p->swapMetadata[i].agingCounter = 0;
      //if(policy2)
      if(SWAP_POLICY == LAPA)
        p->swapMetadata[i].agingCounter = 0xFFFFFFFF;

      //if(policy3)
      if(SWAP_POLICY == SCFIFO)
        addToQueue(i,p);
      // printMetadata(p);
      return 0;
    }
  }
  printf("couldnt find a free spot at swapMetadata\n");
  return -1;
}

int removePage(pagetable_t pagetable, uint64 va){
  struct proc *p = myproc();
  if (p->inExec)
    return 0;
  for(p = proc; p < &proc[NPROC]; p++) {
    if (p->pagetable == pagetable)
      goto foundptbl2;
  }
  return -1;

foundptbl2:

  if (p->pid < 3)
    return 0;

  for (int i = 0; i < MAX_TOTAL_PAGES; i++)
  {
    if (p->swapMetadata[i].va == va){
      p->swapMetadata[i].va = 0;
      p->swapMetadata[i].pa = 0;
      p->swapMetadata[i].inFile = -1;
      p->swapMetadata[i].agingCounter = 0;
      p->pagesInRam--;
      p->pageNum--;
      return 0;
    }
  }
  printf("couldnt find page to remove %d, %s\n", p->pid, p->name);
  return -1;
}


void printMyQueue(struct proc * pr){
  struct  proc * p = pr;
  int current = p->clock_hand;
  int prev = p->swapMetadata[current].prev;
  int old_current =p->swapMetadata[prev].next;
  printf("| %d (last %d)",current,prev);
  old_current = current;
  current = p->swapMetadata[current].next;
  prev = p->swapMetadata[current].prev;
  while(current != p->clock_hand)
  {
    printf("-> %d (%d) ",current,(prev==old_current));
    old_current = current;
    current = p->swapMetadata[current].next;
    prev = p->swapMetadata[current].prev;

  }
  printf("\n");
}


int getQueueSize(){
  return myproc()->queueCurrentSize;
}

int addToQueue(int index, struct proc* pr){
  struct proc* p = pr;
  int old_last=0;

  //invalid index or full queue
  if(index<0 || index > 31 || getQueueSize() ==16)
    return -1;
  // already in queue
  if(p->swapMetadata[index].next != 0)
    return -1;
  if(p->queueCurrentSize==0){
    p->clock_hand=index;
    p->swapMetadata[index].next=index;
    p->swapMetadata[index].prev=index;
  }

  else{
    old_last = p->swapMetadata[p->clock_hand].prev;
    // update first prev
    p->swapMetadata[p->clock_hand].prev = index;
    // link the new one as last in queue
    p->swapMetadata[index].next = p->clock_hand;
    p->swapMetadata[index].prev = old_last;
    //update old last in queue.next
    p->swapMetadata[old_last].next = index;
  }

  p->queueCurrentSize++;
  //debug print
  //printf("index: %d was added\n",index);
  //printMyQueue(p);
  return index;
}

int removeFromQueueByPolicy(struct proc * pr){
  struct proc* p = pr;
  int round=0;
  pte_t *pt;
  //no need to remove page
  if(p->pagesInRam<16){
    printf("error: called removeFromQueueByPolicy() but RAM not 16/16");
    return -1;
  }
  while(round<17){
    round++;
    pt = walk(p->pagetable, p->swapMetadata[p->clock_hand].va, 0);
    // if Access Bit On
    // Turn in off and skip this link
    if (*pt & PTE_A) {
      *pt &= ~PTE_A; // Clear the PTE_A bit
      p->clock_hand = p->swapMetadata[p->clock_hand].next;
    }
    // Access by already off
    else{

      int temp = p->clock_hand;
      p->clock_hand = p->swapMetadata[p->clock_hand].next;
      // printf("before removing?\n");
      // printMyQueue(p);
      // prev.next will be my next now
      p->swapMetadata[p->swapMetadata[temp].prev].next = p->swapMetadata[temp].next;
      // next.prev will be my prev
      p->swapMetadata[p->swapMetadata[temp].next].prev = p->swapMetadata[temp].prev;
      p->swapMetadata[temp].next = 0;
      p->swapMetadata[temp].prev = 0;

      // p->clock_hand = p->swapMetadata[p->clock_hand].next;
      // //  prev.next will by my next now
      // p->swapMetadata[p->swapMetadata[p->clock_hand].prev].next=p->swapMetadata[p->clock_hand].next;
      // // next.prev will by my prev
      // p->swapMetadata[p->swapMetadata[p->clock_hand].next].prev=p->swapMetadata[p->clock_hand].prev;
      // p->swapMetadata[p->clock_hand].next=0;
      // p->swapMetadata[p->clock_hand].prev=0;
      //debug print
      // if(p->clock_hand==16){
      //   printf("");
      // }

      // printf("index %d was removed?\n",temp);
      // printMyQueue(p);
      return p->clock_hand;
      }
  }

  return -1;
}

void printMetadata(struct proc *p){
  pte_t *pte;
  int pte_a;
  
  printf("proc - %d, %s\n", p->pid, p->name);
  printf("proc metadata:\n");
  for (int i = 0; i < MAX_TOTAL_PAGES; i++)
  {
    struct pagingMetadata pm = p->swapMetadata[i];
    if (pm.va != 0 || pm.pa != 0 || pm.inFile != -1){
      pte = walk(p->pagetable,pm.va, 0);
      if ((*pte & PTE_A) == 0)
        pte_a = 0;
      else 
        pte_a = 1;

      printf("i: %d | va: %x | pa: %x | inFile: %d | agingCounter: %x  | pte_a: %d | :\n",i, pm.va, pm.pa, pm.inFile, pm.agingCounter, pte_a);
    }
    // else
    //   printf("i: %d |     unused            va: %x | pa: %x | inFile: %d                | :\n",i, pm.va, pm.pa, pm.inFile);
  }
  
}

void initMetadata(struct proc *p){
  for (int i = 0; i < MAX_TOTAL_PAGES; i++)
  {
    p->swapMetadata[i].va = 0;
    p->swapMetadata[i].pa = 0;
    p->swapMetadata[i].inFile = -1;
    p->swapMetadata[i].agingCounter = 0;
    //if(policy2)
    if(1)
      p->swapMetadata[i].agingCounter = 0xFFFFFFFF;
    p->swapMetadata[i].next = 0;
    p->swapMetadata[i].prev = 0;
  }
  
}
