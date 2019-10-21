#include "types.h"
#include "user.h"
#include "mmu.h"
#include "sched.h"

/**
 * @brief Creates a new thread (via clone) which executes a start routine
 * @param start_routine the function to execute
 * @param arg argument of start_routine
 * @return -1 if error, 0 otherwise
 */
int thread_create(void *(*start_routine)(void*), void *arg){
  setscheduler(SCHED_FIFO, 0); //Avoid the parent to be interupted by child

  char* stack;
  if((stack = malloc(PGSIZE)) == (void*)0){
      printf(2, "thread_create : malloc failed\n");
      return -1;
  }

  if(clone(stack, PGSIZE) == 0){ //Returns 0 in the child process
    start_routine(arg);
    exit();
  }

  return 0;
}

void thread_join(){
  int pid = wait();

  char* stack =  getclonestack(pid);
  free(stack);
}

int main(void) 
{
  setscheduler(SCHED_FIFO, 0); //Don't want the parent to be interupted by child

  char* stack;
  if((stack = malloc(PGSIZE)) == (void*)0){
      printf(2, "malloc failed\n");
      exit();
  }

  if(clone(stack, PGSIZE) == 0){ //Returns 0 in the child process
    printf(1, "Hello from cloned child !\n");
    printf(1, "Child exiting\n");
    exit();
  }


  printf(1, "Parent will wait\n");
  wait();
  printf(1, "Parent exiting.\n");
  
  exit();
}
