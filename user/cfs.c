#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  int i=0;
  int j=0;
  int loop_count=0;
  int main_pid = getpid();
  while(i<3 && getpid() == main_pid)  {
    set_cfs_priority(i);
    fork();
    i++;
  }

//ONLY CHILDRED WILL ENTER
  if(getpid()!=main_pid){
    for(loop_count=1;loop_count<11;loop_count++){
        for(j=1;j<100001;j++){}
        sleep(10);
        // you can make the print sepratly:
        //sleep(getpid()*2);
    }
    int buffer[4];
    get_cfs_stats(getpid(),buffer);
    printf("Child proc pid:%d priority:%d rtime: %d stime: %d retime:%d \n",getpid(),buffer[0],buffer[1],buffer[2],buffer[3]); 
    exit(0,"exit_child");
  }
  // FATHER WAIT
  if(getpid()==main_pid){
    wait(0,0);
    wait(0,0);
    wait(0,0);
  }
  exit(0,"exit");
}
