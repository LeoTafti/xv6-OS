#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return proc->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = proc->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

/**
 * @brief Syscall to set the scheduler and priority level of the currently running process
 * @note Gets arguments and calls setscheduler_lab3, which does the actual work
 * @param policy (int) the new scheduling policy
 * @param plvl (int) the new priority level
 * @return an error code, -1 if error, 0 otherwise.
 */
int
sys_setscheduler(void)
{
  int policy, plvl;
  if(argint(0, &policy) < 0)
    return -1;
  if(argint(1, &plvl) < 0)
    return -1;
  
  setscheduler_lab3(policy, plvl);

  return 0;
}

/**
 * @brief Get the number of the cpu on which the currently running process runs
 * @return The CPU number
 */
int
sys_getcpu(void)
{
  return cpunum();
}

/**
 * @brief Creates a child process which shares some of the parent's execution context / memory
 * @param stack pointer to the beginning of a user space stack for the child (previously allocated)
 * @param size size of the child stack
 * @return child process id if called by parent, 0 if called by the child process, -1 in case of error
 */
int
sys_clone(void)
{
  char* stack;
  int size;

  if(argptr(0, &stack, 4) < 0)
    return -1;
  if(argint(1, &size) < 0)
    return -1;
  
  return clone_lab3(stack, size);
}

/**
 * @brief Gets the cloneStack field for process pid
 * @note Can only be called by the parent process of child pid
 * @param pid the child process id
 * @return cloneStack field for child process pid, -1 if error
 */
int
sys_getclonestack_lab3(void)
{
  int pid;
  if(argint(0, &pid) < 0)
    return -1;
  
  return getclonestack_lab3(pid)
}