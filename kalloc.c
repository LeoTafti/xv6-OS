// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "kalloc.h"

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file

struct kmem kmem;

struct page_info ppages_info[0xE000] = {0}; //Initially unused and refcount is 0.
/**
 * @brief Decrements the page_info.refcount field for corresponding phy page.
 * Frees if refcount reaches 0
 * @param va virtual address of the page, must be page allined
 * @return 1 if page was freed, 0 otherwise
 */
int
kdecref(char *va){
  //Check that the address is page alined
  if((uint)va % PGSIZE != 0){
    panic("kdecref : va not page alined");
  }
  
  struct page_info *pi = &ppages_info[V2P(va) / PGSIZE];
  if(pi->refcount == 0){
    panic("kdecref : refcount == 0 already");
  }

  if((--pi->refcount) == 0){
    kfree(va);
    return 1;
  }

  return 0;
}

int
kinsert(pde_t *pgdir, struct page_info *pp, char *va, int perm)
{

	return 0; //Placeholder so the empty function will compile
}

void
kremove(pde_t *pgdir, void *va)
{

}

struct page_info *
klookup(pde_t *pgdir, void *va, pte_t **pte_store)
{

	return 0;
}


// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
}

/**
 * @brief Marks pages in range (vstart, vend) as used
 * @param vstart virtual address (low), not necessarily page aligned
 * @param vend virtual address (high), not necessarily page aligned
 */
void
kmarkused(void *vstart, void *vend){
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    ppages_info[V2P(p) / PGSIZE].used = 1;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
}

//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  //memset(v, 1, PGSIZE); //Commented out for the lab2 exercise

  if(kmem.use_lock)
    acquire(&kmem.lock);
  struct page_info *pi = &ppages_info[V2P(v) / PGSIZE];
  pi->used = 0;
  pi->next = kmem.freelist;
  kmem.freelist = pi;
  if(kmem.use_lock)
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct page_info *pi;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  pi = kmem.freelist;
  if(pi){
    kmem.freelist = pi->next;
    pi->refcount = 1;
    pi->used = 1;
  }
  if(kmem.use_lock)
    release(&kmem.lock);

  uint index = ((uint)pi - (uint)ppages_info) / sizeof(struct page_info); // Gives us the index of the page_info entry in ppages_info
  return (char*)P2V(index * PGSIZE);                                    // We use this index to find the address of the physical page
}

