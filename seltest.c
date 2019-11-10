#include "types.h"
#include "user.h"
#include "select.h"

/**
 * @brief Computes the nfds parameter used in a call to select
 * @note For test purposes only. Not efficient and assumes unsigned int is 32bit.
 * @return nfds or -1 if both readset and writeset are zero.
 */
int get_nfds(fd_set readset, fd_set writeset){
  for(int i = 32; i >= 0; i--){
    if(FD_ISSET(i, &readset) || FD_ISSET(i, &writeset))
      return i + 1;
  }

  return -1;
}

/**
 * @brief Helper function to print the result of a test to the console
 */
void print_result(char *test_name, int select_retval, int success_condition){
  if(select_retval < 0){
      printf(2, "Test %s : ERROR\n", test_name);
      return;
  }
  
  if(success_condition)
      printf(1, "Test %s : SUCCESS\n", test_name);
  else
      printf(1, "Test %s : FAIL\n", test_name);
}

/**
 * @brief Ensure that pipes and console properly detect when they are readable/writable
 */
void basic_test(){
  int r;

  int p[2];
  int *p_out = &p[0];
  int *p_in = &p[1];

  fd_set readfds, writefds;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);


  // Test 1 : Console should always be writable
  FD_SET(0, &writefds);
  r = select(1, &readfds, &writefds);
  print_result("basic 1", r, r == 1 && FD_ISSET(0, writefds));

  // Test 2 : Console shouldn't be readable before we write anything to it //TODO : is this correct ?
  // Note : we let fd0 still set in writefds s.t. select doesn't wait
  FD_SET(0, &readfds);
  r = select(1, &readfds, &writefds);
  print_result("basic 2", r, r == 1 && FD_ISSET(0, writefds) && !FD_ISSET(0, readfds));

  // Test 3 : Pipe shouldn't be readable when empty, but should be writable
  pipe(p);
  printf(1, "Check that using pointers p_out and p_in works. *p_out = %d\n", *p_out);

  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_SET(*p_out, &readfds);
  FD_SET(*p_in, &writefds);

  printf(1, "Check that get_nfds works properly : *p_out = %d, *p_in = %d, get_nfds(…) = %d\n", *p_out, *p_in, get_nfds(readfds, writefds));

  r = select(get_nfds(readfds, writefds), &readfds, &writefds);
  print_result("basic 3", r, r == 1 && FD_ISSET(*p_in, writefds) && !FD_ISSET(*p_out, &readfds));

  // Test 4 : Pipe should become readable once some data has been written to it
  char txt[] = "Hello world";
  write(*p_in, txt, 5); //Writes "Hello"

  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_SET(*p_out, &readfds);
  FD_SET(*p_in, &writefds); //To check select's behavior with both readable and writable fd's

  r = select(get_nfds(readfds, writefds), &readfds, &writefds);
  print_result("basic 4", r, r == 2 && FD_ISSET(*p_in, writefds) && FD_ISSET(*p_out, &readfds));

  // Test 5 : Pipe should not be readable after it gets emptied again.
  char buf[5];
  read(*p_out, buf, 5);

  //Note : we let *p_in fd set in writefds, s.t. select doesn't wait.
  r = select(get_nfds(readfds, writefds), &readfds, &writefds);
  print_result("basic 5", r, r == 1 && FD_ISSET(*p_in, writefds) && !FD_ISSET(*p_out, &readfds));

  // Test 6 : Pipe should become readable again if we close its write fd(s)
  close(*p_in);

  FD_ZERO(&writefds);
  //*p_out still set in readfds

  r = select(get_nfds(readfds, writefds), &readfds, &writefds);
  print_result("basic 6", r, r == 1 && FD_ISSET(*p_out, &readfds));

  close(*p_in);
  close(*p_out);
}

/**
 * @brief Ensure that select properly waits and wakes up
 */
void waiting_test(){
  int r, nfds;

  fd_set readfds, writefds;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);

  int p[2];
  int *p_out = &p[0];
  int *p_in = &p[1];
  pipe(p);

  FD_SET(*p_out, &readfds);

  //Fork a child. Child sleeps for some time, then puts data into the pipe and sleeps again.
  //Parent calls select right away, trying to read from the end of the pipe (should result in parent waiting);
  if(fork() == 0){
    printf(1, "Child : created, about to sleep.");
    sleep(400);
    printf(1, "Child : writing to pipe\n");
    write(*p_in, "Hello", 6);
    printf(1, "Child : sleeping again\n");
    sleep(400);
    printf(1, "Child : exiting\n");
    close(*p_in);
    close(*p_out);
    exit();
  }

  printf(1, "Parent : calling select\n");
  r = select(*p_out + 1, &readfds, &writefds);
  printf(1, "Parent : back from select\n");

  //TODO : need to wait() somewhere ?

  print_result("waiting 1", r, r == 1 && FD_ISSET(*p_out, &readfds));

  close(*p_in);
  close(*p_out);
}

int main(void){
    //basic_test();
    waiting_test();
    exit();
}