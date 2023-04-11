#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if (argc == 2){
        int polc = *(argv[1]) - '0';
        set_policy(polc);
    }
    else{
        printf("usage: policy X (0 <= X <= 2)\n");
    }
  
  exit(0,"policy Test Ended");
}
