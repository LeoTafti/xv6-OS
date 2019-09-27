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
 * @param va (kernel) virtual address of the page, must be page allined
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

/**
 * @brief Maps the physical page associated with pp at virtual address va
 *        Removes previous mapping, if any.
 *        Allocates and insert a pte entry if needed.
 * @param pgdir (pointer to) the page directory
 * @param pp (pointer to) the page_info of the physical page to map
 * @param va virtual address where to map the page
 * @param perm permission bits
 * @return -1 if page table could not be allocated, 0 otherwise
 */
int
kinsert(pde_t *pgdir, struct page_info *pp, char *va, int perm)
{
  cprintf("Entering kinsert. refcount is %d\n", pp->refcount);
  cprintf("Address of page_info from pp is %x, address of pi compd in kdecref is %x\n", pp, &ppages_info[V2P(va) / PGSIZE]);
  cprintf("Va is : %x\n", va);
  //If there is already a page mapped at va, remove it.
  kremove(pgdir, va); //kremove will do nothing if there is no page mapped.

  cprintf("kremove done. refcount is %d\n", pp->refcount);
  //Then we map pp by calling mappages with the right arguments
  return mappages(pgdir, va, PGSIZE, (pp - ppages_info) * PGSIZE, perm);
}

/**
 * @brief Unmaps the physical page at virtual address va. If no page at va, does nothing.
 * @param pgdir (pointer to) the page directory
 * @param va virtual address
 */
void
kremove(pde_t *pgdir, void *va)
{
  pte_t *pte = walkpgdir(pgdir, va, 0);
  if(pte && (*pte & PTE_P) != 0){
    //Translate pte into kernel va
    char* kva = P2V((uint)*pte & 0x000); // "& 0x000" clears flags, effectively setting offset to 0
    kdecref(kva);
    memset(pte, 0, sizeof(pte));
    tlb_invalidate(pgdir, va);
  }
}

/**
 * @brief Finds and returns (a pointer to the page_info of) the physical page mapped at virtual address va.
 *        If pte_store is non-null, sets it to save the virtual address of the pte for the physical page.
 * @param pgdir (pointer to) the page directory
 * @param va virtual address
 * @param pte_store (pointer to) a pte entry address
 * @return (a pointer to) the page_info of the mapped physical page (if any), NULL otherwise
 */
struct page_info *
klookup(pde_t *pgdir, void *va, pte_t **pte_store)
{
  pte_t *pte = walkpgdir(pgdir, va, 0);
  if(!pte || ((*pte & PTE_P) == 0)){
    return (void*)0;
  }

  if(pte_store)
    *pte_store = pte;
  
  return &ppages_info[PGROUNDDOWN(V2P(pte)) / PGSIZE];
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
    pi->used = 1;
  }
  if(kmem.use_lock)
    release(&kmem.lock);

  char* retval = (char*)0;
  if(pi){
    uint index = pi - ppages_info; // Gives us the index of the page_info entry in ppages_info
    retval = P2V(index * PGSIZE);
  }
  return retval;
}

/**
 * @brief finds page_info from va and increments its refcount
 * @param va (kernel) virtual address of the page, must be page alined
 */
void
increfcount(char* va){
  
  if((uint)va % PGSIZE != 0)
    panic("increfcount : va not page alined");
  
  uint index = V2P(va) / PGSIZE;
  struct page_info *pi = &ppages_info[index];
  if(!pi->used)
    panic("increfcount : not used");

  pi->refcount++;
}

