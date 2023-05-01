#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Define the stack size as a macro
#define STACK_SIZE 4000

// Define a function for the new thread to execute
void *thread_func(void *arg) {
  // Print a message from the new thread
  printf("I'm the new thread created!\n");
  // Exit the new thread
  kthread_exit(1);
  return 0;
}

int main(int argc, char *argv[]) {
  // Allocate a new stack for the new thread to use
  void *stack = malloc(STACK_SIZE);
  if (!stack) {
    printf("Failed to allocate stack for new thread\n");
    exit(1);
  }

  // Create a new thread and start executing the thread_func
  int thread_id = kthread_create(thread_func, stack + STACK_SIZE, STACK_SIZE);
  if (thread_id == -1) {
    printf("Failed to create new thread\n");
    exit(1);
  }
  int status;
  // Wait for the new thread to exit
  kthread_join(thread_id, &status);

  // Free the stack memory
  free(stack);

  exit(0);
}