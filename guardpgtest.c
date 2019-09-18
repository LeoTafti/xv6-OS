#include "types.h"
#include "user.h"
#include "fcntl.h"

int
main(int argc, char *argv[])
{
  printf(1, "This will test the stack guard page once implemented.\n");

  exit();
}