#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if (argc == 2){
        int polc = atoi(argv[1]);
        int ret = set_policy(polc);
        if (ret == 0)
            printf("my new schedPolicy set: %d\n", polc);
    }
    else{
        printf("usage: policy X (0 <= X <= 2)\n");
    }
  
  exit(0,"");
}
