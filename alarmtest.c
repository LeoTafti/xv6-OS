#include "types.h"
#include "stat.h"
#include "user.h"

void periodic();
void callrestore();

int
main(int argc, char *argv[])
{
  int i;
  printf(1, "alarmtest starting\n");
  alarm(10, periodic, callrestore);
  for(i = 0; i < 50*5000000; i++){
    if((i % 5000000) == 0)
      write(2, ".", 1);
  }
  exit();
}

void
periodic()
{
  printf(1, "alarm!\n");
}

void
callrestore(void (*h)(), uint eax, uint ecx, uint edx){
  h();
  __asm__ __volatile__ (
    "movl %0, %%eax;"\
    "movl %1, %%ecx;"\
    "movl %2, %%edx;"
    :
    :"r" (eax), "r" (ecx), "r" (edx)
    :"eax"
  );
}
