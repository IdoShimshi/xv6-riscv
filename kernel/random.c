#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"



struct {
  struct spinlock lock;
  
#define RANDOM_BUF_SIZE sizeof(uint8)
  char buf[RANDOM_BUF_SIZE];
  uint8 seed;
} rand;

//
// read n random bytes into src
// user_dist indicates whether dst is a user
// or kernel address.
//
int
randomwrite(int user_src, uint64 src, int n)
{
    if (n != 1)
        return -1;
    
    char c;
    if(either_copyin(&c, user_src, src, 1) == -1)
        return -1;
    acquire(&rand.lock);
    rand.seed = c;
    release(&rand.lock);


    return 1;
}

//
// decide the next seed, n must be 1
// user_dist indicates whether dst is a user
// or kernel address.
//
int
randomread(int user_dst, uint64 dst, int n)
{
  uint target;
  uint8 randNum;
  char cbuf;

  target = n;
  acquire(&rand.lock);
  while(n > 0){

    randNum = lfsr_char(rand.seed);
    rand.seed = randNum;

    // copy the input byte to the user-space buffer.
    cbuf = randNum;
    if(either_copyout(user_dst, dst, &cbuf, 1) == -1)
      break;

    dst++;
    --n;
  }
  release(&rand.lock);

  return target - n;
}

void
randominit(void)
{
  initlock(&rand.lock, "rand");
  rand.seed = 0x2A;

  // connect read and write system calls
  devsw[RANDOM].read = randomread;
  devsw[RANDOM].write = randomwrite;
}

// Linear feedback shift register
// Returns the next pseudo-random number
// The seed is updated with the returned value
uint8 lfsr_char(uint8 lfsr){
    uint8 bit;
    bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 4)) & 0x01;
    lfsr = (lfsr >> 1) | (bit << 7);
    return lfsr;
}