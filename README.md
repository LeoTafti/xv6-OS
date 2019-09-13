# Homework 2 : My first system call

## 1. System call tracing
In order to get the name of the system call, I created a second array `syscall_names[]`, indexed in the same way as `syscalls[]`. It allows to do a simple table lookup of the name of the syscall, given `num`.

## 2. Date system call
Modifications in order to add the date system call :

* Created the file `date.c` in which I copied the given code, and added a printf statement to standard out (fd 1) which prints the date.
* `Makefile` : added `_date` to `UPROGS`
* `syscall.h` : added syscall number for the date syscall
* `syscall.c` : added a new `sys_date` function to the `syscalls` array (+ to the `syscall_names` array).
* `sysproc.c` : define the `sys_date` function which gets the argument (a `struct rtcdate *` and calls `cmostime()` with it to populate it.
* `usys.S` : added `SYSCALL(date)`
* `user.h` : added `date()` to the list of syscalls.