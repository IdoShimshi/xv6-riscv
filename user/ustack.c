#include "ustack.h"
#include "user/user.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"

typedef long Align;
union header {
  struct {
    uint len;
    uint dealloc_page;
    union header* prev;
  } s;
  Align x;
};

typedef union header Header;

static Header base;
static Header* top;
char* cur_addr;
int now_init = 1;

void* ustack_malloc(uint len1) {
  if (len1 > 512)
    return (void*)-1;

  Header* p;
  char* ret;
  if (now_init){
    now_init = 0;
    cur_addr = sbrk(PGSIZE);
    if (cur_addr == (char*) -1){
      printf("sbrk failed\n");
      return (void*) -1;
    }
    
    base.s.prev = 0;
    base.s.dealloc_page = 0;
    base.s.len = 0;
    top = &base;
  }

  p = (Header*)cur_addr;
  p->s.len = len1 + sizeof(Header);
  
  // no extra page needed
  if (top->s.dealloc_page + p->s.len < PGSIZE) {
    
    p->s.dealloc_page = top->s.dealloc_page + p->s.len;
    
    ret = cur_addr + sizeof(Header);
    cur_addr = cur_addr + p->s.len;
  }
  // let's bring new page
  else {
    char* temp = sbrk(PGSIZE);
    if (temp == (char*) -1){
      printf("sbrk failed\n");
      return (void*) -1;
    }
    // sanitiy check1
    if(temp < cur_addr)
        printf("error:sbrk addr < ustack saved cur_addr\n");

    p->s.dealloc_page = top->s.dealloc_page + p->s.len - PGSIZE;
    ret = cur_addr + sizeof(Header);
    cur_addr = cur_addr + p->s.len;

    // sanitiy check2
    if(temp > cur_addr)
        printf("error:sbrk addr > ustack new cur_addr\n");
  }
  p->s.prev = top;
  top = p;


  return ret;
}

int ustack_free(void) {
  Header* p;
  uint ret;

  if (top == 0 || top == &base){
    printf("ustack_free(): stack empty\n");
    return -1;
  }

  p = top;
  ret = p->s.len - sizeof(Header); // return value
  top = p->s.prev; // pop out of stack

  // printf("dealoc: %d, len: %d\n",p->s.dealloc_page, p->s.len);
  if (((int)p->s.dealloc_page) - ((int)p->s.len) < 0) { // if buffer size crosses bytes used on page free
    sbrk(-PGSIZE);
  }
  cur_addr = cur_addr - p->s.len;

  return ret;
}