#include "spinlock.h"

#define PPNB 0xE000 // number of physical pages = PHYSTOP / PGSIZE

struct page_info {
  struct page_info *next;
  int refcount;
  int used; //0 if unused, other (1) if used
};

struct kmem {
   struct spinlock lock;
   int use_lock;
   struct page_info *freelist;
};