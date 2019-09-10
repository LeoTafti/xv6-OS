#include "types.h"
#include "user.h"
#include "date.h"

int
date (struct rtcdate * r){
  cmostime(r);
  return 0;
}

int
main(int argc, char *argv[])
{
  struct rtcdate r;

  if (date(&r)) {
    printf(2, "date failed\n");
    exit();
  }

  printf(1, "Date : %d/%d/%d time : %d:%d:%d", r.month, r.day, r.year, r.hour, r.minute, r.second);

  exit();
}
