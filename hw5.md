# Homework 5 : User Level Threads

## Thread switching

##### 1. What if you had many cores? Would threads run in parallel?
I don't think threads would run in parallel, since it is still one user process and one kernel thread (even though we provide the abstraction of threads to the user). The kernel can not schedule a single kernel thread on multiple cores.

##### 2. What happens if one thread blocks?
We block forever, since our program depends on a thread calling `thread_yield()` for us to be able to schedule and switch to another user thread.

##### 3. How would you solve these problems?
For problem 1 : Have multiple kernel threads, such that these can be scheduled over multiple cores.

For problem 2 : We could use timer signals to do preemptive scheduling instead and allow switching from one user thread to another every x amout of time.