#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "sched.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
  struct proc *fifo_head; //highest priority, oldest
  struct proc *fifo_tail; //lowest priority, newest
  struct proc *rr_head; //highest priority
  struct proc *rr_tail; //lowest prioirty
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

void enqueue(struct proc *p, int policy); //TODO : clean up
int find(struct proc *p, struct proc **prev, struct proc** head);
//void findandremove(struct proc *p, int policy);

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
// Must hold ptable.lock.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  p->scheduler = SCHED_RR;
  p->next = (void*)0;
  p->priority = PRTY_DFLT;

  enqueue(p, p->scheduler);

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  acquire(&ptable.lock);

  p = allocproc();
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;

  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  acquire(&ptable.lock);

  // Allocate process.
  if((np = allocproc()) == 0){
    release(&ptable.lock);
    return -1;
  }

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    release(&ptable.lock);
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));

  pid = np->pid;

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Remove the process from its scheduling priority list
  //TODO : don't need it (?)
  //cprintf("finandremove from exit\n");
  //findandremove(proc, proc->scheduler);

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  cprintf("Exit jumps to sched\n");
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  for(;;){
    // Enable interrupts on this processor.
    sti();

    acquire(&ptable.lock);
    if(scheduler_lab3(SCHED_FIFO) < 0){ //FIFO tasks take priority over RR
      scheduler_lab3(SCHED_RR);
    }

    release(&ptable.lock);

  }
}

//TODO : doc
void setqueueptrs(struct proc ***head, struct proc ***tail, int policy){
  switch (policy)
  {
  case SCHED_RR:
    *head = &ptable.rr_head;
    *tail = &ptable.rr_tail;
    break;
  case SCHED_FIFO:
    *head = &ptable.fifo_head;
    *tail = &ptable.fifo_tail;
    break;
  default:
    panic("Unexpected policy");
    break;
  }
}

//TODO : doc
//TODO : enqueue assume that the proc isn't already present in queue
// enqueue also asumes that p->priority has already been set
void enqueue(struct proc *p, int policy){
  struct proc **head, **tail;
  setqueueptrs(&head, &tail, policy);

  //Insert p in the right place to respect priority (and fifo behavior, if it applies)
  struct proc *prev, *nxt;
  prev = (void*)0;
  nxt = (*head);
  while(nxt != (void*)0 && nxt->priority >= p->priority){
    prev = nxt;
    nxt = nxt->next;
  }

  if(nxt == (void*)0)  //Rewire tail
    *tail = p;

  if(prev == (void*)0) //Rewire head
    *head = p;
  else
    prev->next = p;
    
  p->next = nxt;
}

//TODO : doc
struct proc* findrunnable(struct proc **prev, struct proc** head){
  struct proc *nxt;

  *prev = (void*)0;
  nxt = (*head);
  while(nxt != (void*)0 && nxt->state != RUNNABLE){
    *prev = nxt;
    nxt = nxt->next;
  }

  if(nxt == (void*)0){ //Not found
    *prev = (void*)0;
  }

  return nxt;
}

void remove(struct proc *p, struct proc *prev, struct proc** head, struct proc** tail); //TODO : cleanup

//TODO : doc
struct proc* dequeue(int policy){
  struct proc **head, **tail;
  setqueueptrs(&head, &tail, policy);

  struct proc *p, *prev;

  if((p = findrunnable(&prev, head)) == (void*)0)
    return (void*)0;

  remove(p, prev, head, tail);

  return p;
}

//TODO : doc
//Searched for proc in linkedlist pointed at by head. Sets prev if found.
//Return -1 (not found) or 0 (found)
int find(struct proc *p, struct proc **prev, struct proc** head){
  struct proc *nxt;

  *prev = (void*)0;
  nxt = (*head);
  while(nxt != p && nxt != (void*)0){
    *prev = nxt;
    nxt = nxt->next;
  }

  if(nxt == (void*)0){ //Not found
    *prev = (void*)0; //(In case caller doesn't look at return value)
    return -1;
  }
  return 0;
}

//TODO : doc
void remove(struct proc *p, struct proc *prev, struct proc** head, struct proc** tail){
  if((*head) == p){
    *head = p->next;
  }
  if(*tail == p){
    *tail = prev;
  }

  if(prev){
    prev->next = p->next;
  }

  p->next = (void*)0;
}

//TODO : doc
//TODO : assumes p is in the queue. If not, silently does nothing.
//TODO : Probably not needed !
void findandremove(struct proc *p, int policy){
  struct proc **head, **tail;
  setqueueptrs(&head, &tail, policy);

  
  struct proc *prev;
  if(find(p, &prev, head) != -1){ //Since we don't have a doubly linked list, we need to find the previous proc by going through the list
    cprintf("Really removing\n");
    remove(p, prev, head, tail);
  }
}

/**
 * @brief Switch execution to given process.
 * @note It is the process's job to release ptable.lock and then reacquire it before jumping back to us
 * @param p the process to run
 */
void
runproc_lab3(struct proc *p)
{
  proc = p;
  switchuvm(p);
  p->state = RUNNING;
  swtch(&cpu->scheduler, p->context);
  switchkvm();

  // Process is done running for now.
  // It should have changed its p->state before coming back.
  proc = 0;
}

/**
 * @brief Finds the next process with given policy to run and runs it
 * @param policy the scheduler policy, either SCHED_RR or SCHED_FIFO
 * @note Assumes that ptable.lock is already held 
 * @return -1 if no runnable process with given policy found, 0 otherwise
 */
int
scheduler_lab3(int policy){
  struct proc *p;
  if((p = dequeue(policy)) == (void*)0)
    return -1;

  runproc_lab3(p);
  
  enqueue(p, p->scheduler);
  return 0;
}

//TODO : doc
//If called with the same params as already set for the process, will still remove and reinsert (possibly changing fifo order)
void setscheduler_lab3(int new_policy, int new_plvl){
  acquire(&ptable.lock);
  
  //int old_policy = proc->scheduler;
  //Remove process from the old queue if necessary.
  //findandremove(proc, old_policy); //TODO : not needed (?)

  //Update proc fields.
  proc->priority = new_plvl;
  proc->scheduler = new_policy;

  //Insert in corresp. priority queue
  enqueue(proc, new_policy);

  release(&ptable.lock);

  yield(); //Will cause the scheduler to be called again, and thus preempt the current running process if necessary
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
