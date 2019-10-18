#include "types.h"
#include "user.h"
#include "mmu.h"
#include "sched.h"

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
    sleep(200);
    exit(); //Causes a trap (PGFault probably)
  }


  printf(1, "Parent will wait\n");
  wait();
  sleep(100);
  printf(1, "Parent exiting.\n");
  
  exit();
}
