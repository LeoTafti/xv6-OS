# Homework 2 : My first system call

## 1. System call tracing
In order to get the name of the system call, I created a second array `syscall_names[]`, indexed in the same way as `syscalls[]`. It allows to do a simple table lookup of the name of the syscall, given `num`.

## 2. Date system call
Modifications in order to add the date system call :

1. Created the file `date.c` in which I copied the given code, and added a printf statement to standard out (fd 1) which prints the date.
2. `Makefile` : added `_date` to `UPROGS`
3. `syscall.h` : added syscall number for the date syscall
4. `syscall.c` : declared and added a new `sys_date` function to the `syscalls` array (+ to the `syscall_names` array).
5. `usys.S` : added `SYSCALL(date)`
6. `user.h` : added `date()` to the list of syscalls.
7. `sysproc.c` : define the `sys_date` function which gets the argument (a `struct rtcdate *` and calls `cmostime()` with it to populate it.

Output :

<center>

![](/Users/Leo/Desktop/GT/Fall/CS3210\ –\ Design\ OS/Homeworks/HW2/date_syscall_output.png)

*3 consecutive calls, with a few seconds inbetween each call*

</center>

## 3. Dup2
### 1. Adding the dup2 system call
Modifications in order to add the dup2 system call : 

1. I repeated modifications 3. 4. 5. 6. from above.
2. `sysfile.c` : defined the `sys_dup2` function which gets the arguments (`oldfd` and associated `struct file` `f` and `newfd`) and tries to allocate `newfd` to `f`.

### 2. Test program

1. Created the `dup2_test.c` file
2. `Makefile` : added `_dup2test` to `UPROGS`

### 3. Using any new fd

It can be shown that any new fd can be used with dup2 using a neat "trick" described [here](https://www.geeksforgeeks.org/dup-dup2-linux-system-call/). The idea is to use dup2 to redirect the standard out to a file, than call `printf` and the output should be written in the file.