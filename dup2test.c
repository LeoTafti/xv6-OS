#include "types.h"
#include "user.h"
#include "fcntl.h"

int
main(int argc, char *argv[])
{
  //Open file and get its file descriptor
  int oldfd;
  if((oldfd = open("dup2test.txt", O_WRONLY)) < 0){
    printf(2, "open failed");
    exit();
  }

  if(write(oldfd, "Writing from first fd\n", 22) < 22){
    printf(2, "first write failed");
    exit();
  }
  
  int newfd = 4; //oldfd should be 3, hence we take another one
  if(dup2(oldfd, newfd) < 0){
    printf(2, "dup2 failed");
    exit();
  }

  if(write(newfd, "Writing from dupped fd\n", 23) < 23){
    printf(2, "second write failed");
    exit();
  }

  exit();
}
