#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "mmu.h"

int
main(int argc, char *argv[])
{
  char buf[PGSIZE + 1];
  memset(buf, 0, sizeof(buf));

  exit();
}
