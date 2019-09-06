# README

### Modifications :

1. printfmt.c : Added support for octal representation using `%o` in `cprintf()`

2. monitor.c : Added a new `backtrace` command to the kernel monitor and implemented the corresponding `mon_backtrace()` function.  
Wrote doxygen doc.

3. kdebug.c : Added line number search in stabs in function `debuginfo_eip()`

### Unit tests :

It suffices for this lab to run the `backtrace` kernel monitor command. The output should be very similar to the one given in the lab1 description *(apart from a slight difference in line numbers, due to the addition of some comments / docummentation)*.

### Answers to exercises (Optional)

Can be found in the **Writeup.pdf** document.