#include "types.h"
#include "user.h"
#include "mmu.h"
#include "sched.h"
#include "free_stack_and_exit.h"

/**
 * @brief sets the scheduler and priority level for the currently running thread
 * @param new_policy the new scheduling policy
 * @param new_plvl the new priority level
 */
void thread_setscheduler(int new_policy, int new_plvl){
  setscheduler(new_policy, new_plvl);
}

/**
 * @brief Creates a new thread (via clone) which executes a start routine
 * @param start_routine the function to execute
 * @param arg argument of start_routine
 * @return -1 if error, 0 otherwise
 */
int thread_create(void *(*start_routine)(void*), void *arg){
  char* stack;
  if((stack = malloc(PGSIZE)) == (void*)0){
      printf(2, "thread_create : malloc failed\n");
      return -1;
  }

  if(clone(stack, PGSIZE) == 0){ //Returns 0 in the child process
    start_routine(arg);

    thread_setscheduler(SCHED_RR, 0); //Required by free_stack_and_exit()
    free_stack_and_exit(stack);
  }

  return 0;
}

/**
 * @brief Waits until any child thread terminates.
 */
void thread_join(){
  wait();
}


/**
 * @brief blocks waiting for stdin input
 * @param unused unused
 * @return null (not used)
 */
void* blockingIO(void* unused){
  //Block on I/O
  printf(1, "Thread %d about to block.\n", getpid());
  char buf[10];
  read(0, buf, 5); //Read 5 bytes from stdin. Will block until 5 bytes given.
  printf(1, "Got input. Not blocking anymore.\n");

  return (void*)0;
}

/**
 * @brief Loops for some time, printing status at regular interval
 * @param thread_name a name to identify the thread
 * @return null (not used)
 */
void* occupy(char* thread_name){
  for(int i = 0; i < 400000; i++){
    if(i % 100000 == 0) //print some feedback on progress
      printf(1, "%s %d–%d/4\n", thread_name, getcpu(), i/100000 + 1);
      //printf(1, "Thread %s on cpu %d – done %d / 4\n", thread_name, getcpu(), i/100000 + 1);
  }

  return (void*)0;
}

struct test_struct {
  void* (*routine)(void*);
  void* arg;
  int sched_policy;
  int p_lvl;
};

//TODO : doc
void* setsched_sleep_do(struct test_struct *ts){
  thread_setscheduler(ts->sched_policy, ts->p_lvl);
  sleep(150);
  ts->routine(ts->arg);
  return (void*)0;
}

int main(void) 
{
  setscheduler(SCHED_FIFO, 0); //Don't want the parent to be interupted by child

  // thread_create(occupy, 0);
  // thread_create(occupy, 0);
  
  // thread_join();
  // thread_join();

  struct test_struct ta = {(void * (*)(void *))occupy, "A", SCHED_FIFO, 3};
  struct test_struct tb = {(void * (*)(void *))occupy, "B", SCHED_FIFO, 4};
  struct test_struct te = {(void * (*)(void *))occupy, "E", SCHED_RR, 2};
  struct test_struct tf = {(void * (*)(void *))occupy, "F", SCHED_RR, 2};
  
  
  thread_create((void * (*)(void *))setsched_sleep_do, &ta);
  thread_create((void * (*)(void *))setsched_sleep_do, &tb);

  for(int i = 0; i < 2; i++){ //Fork processes "C" and "D"
    if(fork() == 0){ //fork() returns 0 in child proc
      switch (i)
      {
      case 0: //Process "C"
        setscheduler(SCHED_RR, 3);
        thread_create((void * (*)(void *))setsched_sleep_do, &te);
        thread_create((void * (*)(void *))setsched_sleep_do, &tf);
        sleep(150);
        occupy("C");
        thread_join();
        thread_join();
        exit();
        break;
      case 1: //Process "D"
        setscheduler(SCHED_RR, 3);
        sleep(150);
        occupy("D");
        exit();
        break;
        
      default:
          break;
      }
    }
  }

  thread_join();
  thread_join();
  thread_join();
  thread_join();

  wait();
  wait();

  exit();
}
