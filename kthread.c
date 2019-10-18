#include "types.h"
#include "user.h"
#include "mmu.h"

int main(void) 
{
  char* stack;
  if((stack = malloc(PGSIZE)) == (void*)0){
      printf(2, "malloc failed\n");
      exit();
  }

  if(clone(stack, PGSIZE) == 0){ //Returns 0 in the child process
    printf(1, "Hello from cloned child !\n");
    printf(1, "Child exiting\n");
  }else{
    sleep(100); //Will prevent the parent thread from exiting before child exits (no guarantee here, but works in practice)
    printf(1, "Parent exiting.\n");
  }
  
  exit();
}