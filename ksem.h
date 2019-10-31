#ifndef __INCLUDE_ksem_h_
#define __INCLUDE_ksem_h_

#include "spinlock.h"

struct ksem {
  int count;
  struct spinlock lock;
};

void ksem_init(struct ksem *sem, int init);
void ksem_up(struct ksem *sem);
void ksem_down(struct ksem *sem);

#endif  // __INCLUDE_ksem_h_
