#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int longFunction(){
  long long i, j, n, k;
    n = 100000; // number of iterations
    k = 0;

    for (i = 0; i < n; i++) {
      if (i % 10000 ==0){
        sleep(10);
      }
        
      for (j = 0; j < n/2; j++) {
          k = k + i + j;
      }
    }
    return 0;
}

int main(int argc, char *argv[])
{
  // set_policy(2);
  int num_of_procs = 3;
  if (argc == 2){
    num_of_procs = atoi(argv[1]);
  }
  int i=0;
  // int j=0;
  int main_pid = getpid();
  for(i=0; i < num_of_procs && getpid() == main_pid; i++) {
    set_cfs_priority(i%3);
    
    // sleep(10);
    if (fork() ==0){
      set_ps_priority(2*(i%3)+5);
    }
  }

//ONLY CHILDRED WILL ENTER
  if(getpid()!=main_pid){


    // for(i=0;i<1000000;i++){
    //     if (i % 100000 == 0)
    //       sleep(10);
    // }
    
    longFunction();
    
    int buffer[4];
    get_cfs_stats(getpid(),buffer);
    printf("Child proc pid:%d priority:%d rtime: %d stime: %d retime:%d \n",getpid(),buffer[0],buffer[1],buffer[2],buffer[3]); 
    exit(0,"exit_child");
  }
  // FATHER WAIT
  if(getpid()==main_pid){
    for (i =0; i< num_of_procs; i++){
      wait(0,0);
    }
  }
  exit(0,"exit");
}
