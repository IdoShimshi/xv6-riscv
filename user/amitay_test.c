#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  set_ps_priority(69);
  exit(0,"amitay_test_ended");
}
