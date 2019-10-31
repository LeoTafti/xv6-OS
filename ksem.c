/**
 * Kernel level semaphore implementation
 */

#include "types.h"
#include "ksem.h"

#include "defs.h"
#include "mmu.h"

/**
 * Initializes the ksem
 * @param ksem -- semaphore to init
 * @param init -- the inital value for the semaphore
 */
void ksem_init(struct ksem *sem, int init) {
  sem->count = 0;
  initlock(&sem->lock, "semlock");
}

/**
 * Ups the semaphore, if there were sleepers, it wakes up 1
 * @param ksem -- semaphore to up
 */
void ksem_up(struct ksem *sem) {
  acquire(&sem->lock);

  if (sem->count < 0) {
    panic("ksem_up on -1");
  }

  if (sem->count == 0) {
    wakeup(sem);
  }

  sem->count++;

  release(&sem->lock);
}

/**
 * Downs the semaphore.  If the semaphore value is 0, sleeps until upped
 * @param ksem -- semaphore to down
 */
void ksem_down(struct ksem *sem) {
  acquire(&sem->lock);
  if (sem->count < 0) {
    panic("ksem_down on -1");
  }

  while (sem->count == 0) {
    sleep(sem, &sem->lock);
  }

  sem->count--;

  release(&sem->lock);
}

