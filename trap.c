#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "kalloc.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

extern struct page_info ppages_info[];

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(proc->killed)
      exit();
    proc->tf = tf;
    syscall();
    if(proc->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpunum() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpunum(), tf->cs, tf->eip);
    lapiceoi();
    break;

  //PAGEBREAK: 13
  default:
    if(proc == 0 || ((tf->cs&3) == 0 && tf->trapno != T_PGFLT)){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpunum(), tf->eip, rcr2());
      panic("trap");
    }

    if(tf->trapno == T_PGFLT){
      //We check to see if the faulting page had its PTE_COW bit set, and that it was a write
      struct page_info *pi;
      pte_t *pte;
      if((pi = klookup(proc->pgdir, (char*)rcr2(), &pte)) != 0
          && (*pte & PTE_COW)
          && (tf->err & ERR_W)){
        if(pi->refcount == 1){ //The current process has exclusive access (fault is only due to past COW)
          //Simply rewrite permission bits
          uint newFlags = (PTE_FLAGS(*pte) & ~PTE_COW) | PTE_W;
          *pte = PTE_ADDR(*pte) | newFlags;
          tlb_invalidate(proc->pgdir, (char*)rcr2());
        }else{
          //Allocate a page, copy faulting page content to it, map it.
          char* newpgkva;
          if((newpgkva = kalloc()) == 0){
            cprintf("COW : kalloc() failed. Killing process.\n");
            goto kill;
          }

          char* faultpgkva = (char*)P2V((pi - ppages_info) * PGSIZE);
          cprintf("Physical address of page being copied : %x\n", (pi - ppages_info) * PGSIZE);
          memmove(newpgkva, faultpgkva, PGSIZE);

          uint flags = PTE_FLAGS(*pte);
          struct page_info *newpgpi = &ppages_info[V2P(newpgkva) / PGSIZE];
          if(kinsert(proc->pgdir, newpgpi, (char*)rcr2(), (flags & ~PTE_COW) | PTE_W) != 0){ //Mark the new page writable and not COW
            cprintf("COW : kinsert() failed. Killing process.\n");
            goto kill;
          }

          //Edit faulting page refcount
          kdecref(faultpgkva);
        }
      }else{
        panic("trap.c : unexpected page fault");
      }
    }else{

kill:
      // Otherwise in user space, assume process misbehaved.
      cprintf("pid %d %s: trap %d err %d on cpu %d "
              "eip 0x%x addr 0x%x--kill proc\n",
              proc->pid, proc->name, tf->trapno, tf->err, cpunum(), tf->eip,
              rcr2());
      proc->killed = 1;
    }
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(proc && proc->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(proc && proc->state == RUNNING && tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(proc && proc->killed && (tf->cs&3) == DPL_USER)
    exit();
}
