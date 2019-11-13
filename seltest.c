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
  print_result("basic 1", r, r == 1 && FD_ISSET(0, &writefds));

  // Test 2 : Console shouldn't be readable if we don't write anything to it
  // Note : we let fd0 still set in writefds s.t. select doesn't wait
  FD_SET(0, &readfds);
  r = select(1, &readfds, &writefds);
  print_result("basic 2", r, r == 1 && FD_ISSET(0, &writefds) && !FD_ISSET(0, &readfds));

  // Test 3 : Pipe shouldn't be readable when empty, but should be writable
  pipe(p);

  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_SET(*p_out, &readfds);
  FD_SET(*p_in, &writefds);

  r = select(get_nfds(readfds, writefds), &readfds, &writefds);
  print_result("basic 3", r, r == 1 && FD_ISSET(*p_in, &writefds) && !FD_ISSET(*p_out, &readfds));

  // Test 4 : Pipe should become readable once some data has been written to it
  char txt[] = "Hello world";
  write(*p_in, txt, 5); //Writes "Hello"

  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_SET(*p_out, &readfds);
  FD_SET(*p_in, &writefds); //To check select's behavior with both readable and writable fd's

  r = select(get_nfds(readfds, writefds), &readfds, &writefds);
  print_result("basic 4", r, r == 2 && FD_ISSET(*p_in, &writefds) && FD_ISSET(*p_out, &readfds));

  // Test 5 : Pipe should not be readable after it gets emptied again.
  char buf[5];
  read(*p_out, buf, 5);

  //Note : we let *p_in fd set in writefds, s.t. select doesn't wait.
  r = select(get_nfds(readfds, writefds), &readfds, &writefds);
  print_result("basic 5", r, r == 1 && FD_ISSET(*p_in, &writefds) && !FD_ISSET(*p_out, &readfds));

  // Test 6 : Pipe should become readable again if we close its write fd(s)
  close(*p_in);

  FD_ZERO(&readfds);
  FD_ZERO(&writefds);

  FD_SET(*p_out, &readfds);

  r = select(get_nfds(readfds, writefds), &readfds, &writefds);
  print_result("basic 6", r, r == 1 && FD_ISSET(*p_out, &readfds));

  close(*p_in);
  close(*p_out);
}

/**
 * @brief Ensure that select properly waits and wakes up, with pipes
 */
void waiting_test_1(){
  int r;

  fd_set readfds, writefds;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);

  int p[2];
  int *p_out = &p[0];
  int *p_in = &p[1];
  pipe(p);

  FD_SET(*p_out, &readfds);
  //Fork a child. Child sleeps for some time, then puts data into the pipe and sleeps again.
  //Parent calls select right away, checking if the pipe is readable (should result in parent waiting);
  if(fork() == 0){
    printf(1, "Child : created, about to sleep.\n");
    sleep(200);
    printf(1, "Child : writing to pipe\n");
    write(*p_in, "Hello", 6);
    printf(1, "Child : sleeping again\n");
    sleep(200);
    printf(1, "Child : exiting\n");
    close(*p_in);
    close(*p_out);
    exit();
  }

  printf(1, "Parent : calling select\n");
  r = select(*p_out + 1, &readfds, &writefds);
  printf(1, "Parent : back from select\n");

  print_result("waiting_test_1", r, r == 1 && FD_ISSET(*p_out, &readfds));

  close(*p_in);
  close(*p_out);

  wait(); //Wait for child to exit
}

/**
 * @brief Ensure that select properly waits and wakes up, with console
 */
void waiting_test_2(){
  int r;

  fd_set readfds, writefds;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);

  FD_SET(0, &readfds);

  printf(1, "Calling select\n");
  r = select(1, &readfds, &writefds);
  printf(1, "Back from select, now reading.\n");

  char buf[20];
  read(0, buf, 20);

  printf("Just read : \"%s\"\n", buf);

  print_result("waiting_test_2", r, r == 1 && FD_ISSET(0, &readfds));
}

void waiting_test_3(){
  int r;

  fd_set readfds, writefds;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);

  int p[2];
  int *p_out = &p[0];
  int *p_in = &p[1];
  pipe(p);

  //Fork two children. Both call select checking if the pipe is readable.

  for(int i = 1; i<=2; i++){
    if(fork() == 0){
      FD_SET(*p_out, &readfds);
      printf(1, "Child %d : created, calling select.\n", i);
      r = select(*p_out + 1, &readfds, &writefds);
      printf(1, "Child %d : back from select.\n", i);
      
      print_result("waiting_test_3 (child)", r, r == 1 && FD_ISSET(*p_out, &readfds));

      close(*p_in);
      close(*p_out);
      exit();
    }
  }

  printf(1, "Parent : about to sleep.\n");
  sleep(200);

  printf(1, "Parent : writing to pipe.\n");
  write(*p_in, "Hello", 6);

  printf(1, "Parent : waiting for children.\n");
  wait(); //Wait for children to exit
  wait();

  close(*p_in);
  close(*p_out);
}

int main(void){
    //basic_test();
    //waiting_test_1();
    waiting_test_2();
    //waiting_test_3();
    exit();
}
