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
  print_result("basic_test_1", r, r == 1 && FD_ISSET(0, &writefds));

  // Test 2 : Console shouldn't be readable if we don't write anything to it
  // Note : we let fd0 still set in writefds s.t. select doesn't wait
  FD_SET(0, &readfds);
  r = select(1, &readfds, &writefds);
  print_result("basic_test_2", r, r == 1 && FD_ISSET(0, &writefds) && !FD_ISSET(0, &readfds));

  // Test 3 : Pipe shouldn't be readable when empty, but should be writable
  pipe(p);

  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_SET(*p_out, &readfds);
  FD_SET(*p_in, &writefds);

  r = select(get_nfds(readfds, writefds), &readfds, &writefds);
  print_result("basic_test_3", r, r == 1 && FD_ISSET(*p_in, &writefds) && !FD_ISSET(*p_out, &readfds));

  // Test 4 : Pipe should become readable once some data has been written to it
  char txt[] = "Hello world";
  write(*p_in, txt, 5); //Writes "Hello"


  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_SET(*p_out, &readfds);
  FD_SET(*p_in, &writefds); //To check select's behavior and return value with both readable and writable fd's set

  r = select(get_nfds(readfds, writefds), &readfds, &writefds);
  print_result("basic_test_4", r, r == 2 && FD_ISSET(*p_in, &writefds) && FD_ISSET(*p_out, &readfds));

  // Test 5 : Pipe should not be writable when full
  // Note : 512 is PIPESIZE, but PIPESIZE wasn't visible from here (since defined in pipe.c)
  // and I didn't want to change it only for the purpose of writing this test.
  char txt2[512] = "bBGv1VVXCSUw6Di87W6cbB71eiiz2nCRwbqXV7K6p95O1ojtTrsI6BVVWz1X1gLGxLS9hcF0KiSI4ptXh7SJWf0RK09Dps0ODE2DWVCM0TLkz8u0YD4il7FUZGk1lHA2sx37b1GWmvBCNGInK5S8uer2b6fqcSH8oGjoXhAB8BYDXbNBQgw2ZBL7DCxbxMKQcT4ITOrdSJyCKasIfn1WUcZON69DzszteRYcpXfzz1dwP6kCiXlgqanoMxdGQFSsi7c3NYMdbj7HjwimaCakE5r7XwVVGXjzP1zPwye3WSP9Ejc8uKJ7KkGiUkPeybLuu3d1JV2aPtZXlWJHzyiQCFFJXriINgzRS0vg6vVwIhVMqEs4bgkWWsPPB9alQUc9EwxWFsUaA9LkdKRutraoYzB8IJ7FfOmHmF5ls2r39tumWGGUvsRWR4I6nG2xP4EeE1Y0JeJZR1BPfHFuYXv3W8UwHFeWaeQsgkwiGzNULAVxw375M1R0JZj6cVuFfOaf";
  write(*p_in, txt2, 512 - 5); //"Hello" is still in the pipe

  r = select(get_nfds(readfds, writefds), &readfds, &writefds);
  print_result("basic_test_5", r, r == 1 && !FD_ISSET(*p_in, &writefds) && FD_ISSET(*p_out, &readfds));

  // Test 6 : Pipe should not be readable after it gets emptied again, but should be writable.
  read(*p_out, txt2, 512);

  FD_SET(*p_in, &writefds);

  r = select(get_nfds(readfds, writefds), &readfds, &writefds);
  print_result("basic_test_6", r, r == 1 && FD_ISSET(*p_in, &writefds) && !FD_ISSET(*p_out, &readfds));

  // Test 7 : Pipe should become readable again if we close its write fd(s)
  close(*p_in);

  FD_ZERO(&readfds);
  FD_ZERO(&writefds);

  FD_SET(*p_out, &readfds);

  r = select(get_nfds(readfds, writefds), &readfds, &writefds);
  print_result("basic_test_7", r, r == 1 && FD_ISSET(*p_out, &readfds));

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

  printf(1, "Just read : %s", buf);

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

void unregister_test(){
  int r;

  fd_set readfds, writefds;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);

  int p1[2];
  int *p_out_1 = &p1[0];
  //int *p_in_1 = &p1[1];
  pipe(p1);

  int p2[2];
  int *p_out_2 = &p2[0];
  int *p_in_2 = &p2[1];
  pipe(p2);

  //p_out_1 isn't readable, we make p_out_2 readable by writing to pipe 2
  char txt[] = "Hello world";
  write(*p_in_2, txt, 5);

  FD_SET(*p_out_1, &readfds);
  FD_SET(*p_out_2, &readfds);

  for(int i = 0; i < 2*MAX_NB_SLEEPING; i++){ //If we don't unregister properly, this will exhaust ressources and fail
    r = select(get_nfds(readfds, writefds), &readfds, &writefds);
    if(r < 0)
      printf(2, "unregister_test : select failed");
  }

  print_result("unregister_test", r, r == 1 && !FD_ISSET(*p_out_1, &readfds) && FD_ISSET(*p_out_2, &readfds));
}

void err_chk_test(){
  int r;

  fd_set readfds, writefds;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);

  //Zero readfds / writefds, shouldn't wait
  r = select(0, &readfds, &writefds);
  print_result("err_chk_test_1", r, r == 0);

  //Negative nfds
  r = select(-1, &readfds, &writefds);
  print_result("err_chk_test_2", 0, r == -1); //Note : I set retval to 0 here instead of r to avoid printing "ERROR"

  //nfds > MAX_NFDS
  r = select(MAX_NFDS + 1, &readfds, &writefds);
  print_result("err_chk_test_3", 0, r == -1); //Note : as above
}

int main(void){
    basic_test();
    //waiting_test_1();
    //waiting_test_2();
    //waiting_test_3();
    //unregister_test();
    //err_chk_test();
    exit();
}
