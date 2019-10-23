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

  printf(1, "Thread %d cloning with stack %p\n", getpid(), stack);

  if(clone(stack, PGSIZE) == 0){ //Returns 0 in the child process
    start_routine(arg);
    exit();
  }

  return 0;
}

/**
 * @brief Waits until any child thread terminates.
 */
void thread_join(){
  //FIXME: This won't work (for good reasons). Fix once I know how to make it work
  //int pid = wait();
  //char* stack =  getclonestack(pid);
  //printf(1, "Thread %d will free stack %p for thread %d\n", getpid(), stack, pid);
  //free(stack);
}

/**
 * @brief blocks waiting for stdin input
 * @param unused unused
 */
void* blockingIO(void* unused){
  //Block on I/O
  printf(1, "Thread %d about to block.\n", getpid());
  char buf[10];
  read(0, buf, 10); //Read 10 bytes from stdin. Will block until 10 bytes given.
  printf(1, "Not reaching here due to blocking I/O\n");

  return (void*)0;
}

/**
 * @brief Loops for some time, printing status at regular interval
 * @param unused unused
 */
void* occupy(void* unused){
  for(int i = 0; i < 10000; i++){
    if(i % 2000 == 0) //print some feedback on progress
      printf(1, "Thread %d – done %d / 5\n", getpid(), i/2000 + 1);
  }

  return (void*)0;
}

int main(void) 
{
  setscheduler(SCHED_FIFO, 0); //Don't want the parent to be interupted by child

  thread_create(occupy, 0);
  //thread_create(blockingIO, 0);
  
  thread_join();
  thread_join();

  exit();
}
