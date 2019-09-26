#include <stdbool.h>

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "monitor.h"
#include "kalloc.h"

#define LAB 2

static void startothers(void);
static void mpmain(void)  __attribute__((noreturn));
extern pde_t *kpgdir;
extern char end[]; // first address after kernel loaded from ELF file
extern struct kmem kmem;
extern struct page_info ppages_info[];

#if LAB >= 2    // ...then leave this code out.
#elif LAB >= 1
// Test the stack backtrace function (lab 1 only)
void
test_backtrace(int x)
{
  cprintf("entering test_backtrace %d\n", x);
  if (x > 0)
    test_backtrace(x-1);
  else
    mon_backtrace(0, 0, 0);
  cprintf("leaving test_backtrace %d\n", x);
}
#endif


//Auxillary methods for the two freelist tests below.

/**
 * @brief Iterates over the freelist linked list, looking for a page_info matching the given index in memory
 * @param index Index of the page in memory
 * @return true if found, false otherwise
 */
bool
is_in_freelist(uint index){
  struct page_info* pi = kmem.freelist;
  while(pi != (void*)0){
    uint i = pi - ppages_info;
    if(i == index)
      return true;

    pi = pi->next;
  }
  return false;
}

/**
 * @brief Checks that the pages from V2P(PGROUNDUP(end)) to PGROUNDDOWN(p_upto) are found in freelist
 * @param p_upto The physical address upto which to perform the check
 * @return true if all pages were found in freelist, false otherwise
 */
bool
check_extmem_mapped(uint p_upto){
  for(uint index = V2P(PGROUNDUP((uint)end))/PGSIZE; index < PGROUNDDOWN(p_upto)/PGSIZE; index++){
    if(!is_in_freelist(index)){
      cprintf("Index not found %d, corresponding phy addr %x\n", index, index*PGSIZE);
      return false;
    }
  }

  return true;
}

bool
test_page_free_list()
{
  //Check the page free list is not corrupted
  //Check that the pages that should not be free are not on the list of free pages

  struct page_info* pi = kmem.freelist;
  while(pi != (void*)0){
    uint index = pi - ppages_info;
    uint addr = index * PGSIZE;
    if(addr > 4 * MB           //Checks that no page are above the 4MB limit
        || addr < V2P(end)){   //Checks that no page on the list of free pages comes from a region where it shouldn't be free
      cprintf("FAIL : Page with addr %x, not in bounds\n", addr);
      return false;
    }
    pi = pi->next;
  }
  //Assert that the first part of physical memory have been mapped to free pages
  return check_extmem_mapped(4 * MB);

}

bool
test_page_free_list_ext()
{
  bool success = test_page_free_list();
    if(!success)
      return false;

  //Assert all unused physical memory have been mapped to free pages
  return check_extmem_mapped(PHYSTOP);
}

bool
test_page_alloc()
{
	//Count the number of free pages
  struct page_info *pi = kmem.freelist;
  uint cnt = 0;
  while(pi != (void*)0){
    cnt++;
    pi = pi->next;
  }

  cprintf("There are %d free pages (= nb of pages on the freelist)\n", cnt);

	//Allocate a few pages with kalloc
  const uint nbpages = 10;
  char* pages[nbpages];
  for(int i = 0; i < nbpages; i++){
    if((pages[i] = kalloc()) == 0)
      panic("test_page_alloc() : error allocating a few pages");
    increfcount(pages[i]);
  }

  bool success = true;

	//Assert all pages are different
  for(int i = 0; i < nbpages; i++){
    for(int j = 0; j < i; j++){
      if(pages[i] == pages[j]){
        cprintf("FAIL : All pages %d and %d are not different\n", i, j);
        success = false;
      }
    }
  }
  if(!success)
    return false;
  cprintf("SUCCESS : All pages are different\n");

	//Assert that the physical addresses are within expected bounds
  success = true;
  for(int i = 0; i < nbpages; i++){
    if(V2P(pages[i]) < EXTMEM || V2P(pages[i]) >= PHYSTOP){
      cprintf("FAIL : Page %d is not whithin bounds with phy addr %x\n", i, V2P(pages[i]));
      success = false;
    }
  }
  if(!success)
    return false;
  cprintf("SUCCESS : ALL phy addr. are within bounds\n");

	//Disable the freelist by saving it to a temporary variable and set freelist to null
  struct page_info *t_freelist = kmem.freelist;
  kmem.freelist = (void*)0;

	//Assert kalloc returns 0 (null)
  char* ptr;
  if((ptr = kalloc()) != 0){
    cprintf("FAIL : kalloc returned a non-null value %p\n", ptr);
    return false;
  }
  cprintf("SUCCESS : kalloc returns 0 (null), as expected\n");

	//Free pages allocated in second commment
  for(int i = 0; i < nbpages; i++){
    kfree(pages[i]);
  }

	//Reallocate pages, assert they are reallocated in reverse order
  char* realloc_pages[nbpages];
  for(int i = nbpages-1; i >= 0; i--){ //We fill this array in REVERSE order
    if((realloc_pages[i] = kalloc()) == 0)
      panic("test_page_alloc() : error REallocating a few pages");
    // increfcount(realloc_pages[i]); // TODO : remove if we don't bother with refcount here (ie. if we keep kfree() calls too)
  }

  success = true;
  for(int i = 0; i < nbpages; i++){ //Then check that pages[] and realloc_pages[] hold the same addresses
    if(pages[i] != realloc_pages[i]){
      cprintf("FAIL : pages and realloc_pages entries at index %d are not the same\n", i);
      success = false;
    }
  }

  if(!success)
    return false;
  cprintf("SUCCESS : pages were reallocated in reverse order\n");

	//Assert that once all pages are reallocated, kalloc again returns 0
  if(kalloc() != 0){
    cprintf("FAIL : kalloc returned a non-null value (2)\n");
    return false;
  }
  
  cprintf("SUCCESS : kalloc returns 0 (null), as expected (2)\n");

	//Set one page to known junk values
  char junk = 0xAA;
  memset(pages[0], junk, PGSIZE);

	//Free the page, reallocate it.  Assert that the page is the same one with the same junk values.
  kfree(pages[0]);
  char* page0 = kalloc();
  if(page0 != pages[0]){
    cprintf("FAIL : Freeing pages[0] then allocating doesn't return the same page.\n");
    return false;
  }

  success = true;
  for(int i = 0; i < PGSIZE; i++){
    if(page0[i] != junk){
      cprintf("FAIL : page0 byte nb %d isn't set to junk value %x\n", i, junk);
      success = false;
    }
  }
  if(!success)
    return false;
  cprintf("SUCCESS : page0 is the same as page[0], filled with all junk values.\n");

	//Restore the page free list saved to the temporary variable in fifth step.  Free the pages allocated in this test.
  kmem.freelist = t_freelist;
  for(int i = 0; i<nbpages; i++){
    kfree(pages[i]);
  }

	//Assert the number of free pages is the same as in the beginning.
  pi = kmem.freelist;
  uint cnt2 = 0;
  while(pi != (void*)0){
    cnt2++;
    pi = pi->next;
  }

  if(cnt != cnt2){
    cprintf("FAIL : cnt = %d, cnt2 = %d", cnt, cnt2);
    return false;
  }
  
  cprintf("SUCCESS : Same number of pages\n");
  return true;
}

int
test_page()
{
	//Allocate a few pages p1, p2, p3
  char* p1 = kalloc();
  char* p2 = kalloc();
  char* p3 = kalloc();

	//Assert that they are all non-zero and different from one another
  if(p1 == 0 || p2 == 0 || p3 == 0
      || p1 == p2 || p1 == p3 || p2 == p3){
    cprintf("FAIL : Pages are either not all non-zero or not all different from one another.\n");
    return false;
  }

  increfcount(p1);
  increfcount(p2);
  increfcount(p3);

  cprintf("SUCCESS : Pages are all non-zero and different from one another.\n");

  //For later use, we store the corresp. page_info entries
  struct page_info pi1 = ppages_info[V2P(p1) / PGSIZE];
  struct page_info pi2 = ppages_info[V2P(p2) / PGSIZE];
  struct page_info pi3 = ppages_info[V2P(p3) / PGSIZE];

	//Save the free page list to a temporary variable.  Set free page list to zero.
  struct page_info *t_freelist = kmem.freelist;
  kmem.freelist = (void*)0;

	//Assert that klookup(0x0) == 0
  if(klookup(0x0) != 0){
    cprintf("FAIL : klookup(0x0) != 0\n");
    return false;
  }
  cprintf("SUCCESS : klookup(0x0) == 0\n");

	//Assert that you can not allocate a page table with kinsert
  
  //Try to kinsert the page_info of p1, with virt addr one page below KERNBASE
  if(kinsert(proc->pgdir, &pi1, KERNBASE - PGSIZE, 0) != -1){
    cprintf("FAIL : kinsert WAS ABLE to allocate a page table (and we didn't expect it).\n");
    return false;
  }
  cprintf("SUCCESS : kinsert was NOT able to allocate a page table, as expected.\n");

	//Free page p1, kinsert the physical page p2 at 0x0. Assert the operation succeeded.
  kdecref(p1); //TODO : or kfree() ?
  if(kinsert(proc->pgdir, &pi2, 0x0, 0) != 0){
    cprintf("FAIL : kinsert for p2 didn't succeed.\n");
    return false;
  }
  cprintf("SUCCESS : kinsert for p2 succeeded.");

	//Assert that p1 is the page table from the previous step.  Assert p2 is in that page table.
  struct page_info *pi;
  pte_t *pte_p2;
  if((pi = klookup(proc->pgdir, 0x0, &pte_p2)) == 0){
    cprintf("FAIL : nothing mapped at 0x0.\n");
    return false;
  }
  if(pi != &pi1){
    cprintf("FAIL : p1 isn't the physical page used to map p2.\n");
    return false;
  }
  //p2 is in the page table if its address (pte_store) is less than PGSIZE away from the begining of the page table, which is allocated in p1
  if(!(pte_p2 >= V2P(p1) && pte_p2 < V2P(p1) + PGSIZE)){ //TODO : not sure about this check.
    cprintf("FAIL : p2 pte isn't in the page table stored in p1.\n");
    return false;
  }
  cprintf("SUCCESS : p1 was allocated to use as page table, and p2's pte is in it\n");
	
  //Asset that p1 and p2 have a ref count of 1.
  if(pi1.refcount != 1 || pi2.refcount != 1){
    cprintf("FAIL : p1 or p2 hasn't a refcount of 1. p1 : %d, p2 : %d\n", pi1.refcount, pi2.refcount);
    return false;
  }
  cprintf("SUCCESS : p1 and p2 have a refcount of 1.\n");

	//Kinsert p3 at 0x1000.
  if(kinsert(proc->pgdir, &pi3, 0x1000, 0) != 0){
    cprintf("FAIL : Couldn't kinsert p3 at 0x1000.\n");
    return false;
  }

	//Assert that p3 is also in the page table stored at p1.  Assert p3 has a ref count of 1.
  //We proceed as above TODO : IF ABOVE check marked with a todo is wrong, this copy pasted code will be too
  pte_t *pte_p3;
  if((pi = klookup(proc->pgdir, 0x1000, &pte_p3)) == 0){
    cprintf("FAIL : nothing mapped at 0x1000.\n");
    return false;
  }
  if(pi != &pi1){
    cprintf("FAIL : p1 isn't the physical page used to map p3.\n");
    return false;
  }
  if(!(pte_p3 >= V2P(p1) && pte_p3 < V2P(p1) + PGSIZE)){ //TODO : not sure about this check.
    cprintf("FAIL : p3 pte isn't in the page table stored in p1.\n");
    return false;
  }
  cprintf("SUCCESS : p3's pte is in the page table in p1.\n");

  if(pi3.refcount != 1){
    cprintf("FAIL : p3's refcount is %d instead of 1.\n", pi3.refcount);
    return false;
  }
  cprintf("SUCCESS : p3's refcount is 1.\n");

	//Reinsert p3 at 0x1000.
  if(kinsert(proc->pgdir, &pi3, 0x1000, 0) != 0){
    cprintf("Fail : Couldn't re-kinsert p3 at 0x1000.\n");
    return false;
  }

	//Assert that p3 is still in the page table stored at p1.  Assert p3 still has a reference count of 1.
  //TODO : copy pasted here again. Maye create a function ? but it would be a bit weird
  if((pi = klookup(proc->pgdir, 0x1000, &pte_p3)) == 0){
    cprintf("FAIL : nothing mapped at 0x1000.(2)\n");
    return false;
  }
  if(pi != &pi1){
    cprintf("FAIL : p1 isn't the physical page used to map p3.(2)\n");
    return false;
  }
  if(!(pte_p3 >= V2P(p1) && pte_p3 < V2P(p1) + PGSIZE)){ //TODO : not sure about this check.
    cprintf("FAIL : p3 pte isn't in the page table stored in p1.(2)\n");
    return false;
  }
  cprintf("SUCCESS : p3's pte is in the page table in p1.(2)\n");

  if(pi3.refcount != 1){
    cprintf("FAIL : p3's refcount is %d instead of 1.(2)\n", pi3.refcount);
    return false;
  }
  cprintf("SUCCESS : p3's refcount is still 1.\n");

	//Assert you can no longer allocate any more pages.
  char* ptr;
  if((ptr = kalloc()) != 0){
    cprintf("FAIL : could allocate more pages. kalloc returned non-null value %p\n", ptr);
    return false;
  }
  cprintf("SUCCESS : cannot allocate any more pages. kalloc returns 0 (null)\n");

	//Change the permissions on the pages with kinsert. Assert permissions were changed correctly.
  if(kinsert(proc->pgdir, &pi2, 0x0, PTE_W) != 0){
    cprintf("FAIL : couldn't call kinsert again for p2\n");
    return false;
  }
  //pte_p2 PTE_W should be set
  if(*pte_p2 & PTE_W == 0){
    cprintf("FAIL : pte_p2 PTE_W bit still not set.\n");
    return false;
  }
  cprintf("SUCCESS : Could change permissions – pte_p2 PTE_W bit was set.\n");

	//Do a remap with fewer permissions on the pages with kinsert.  Assert permissions were changed correctly.
  //TODO : copy pasted code from just above. Don't know if "remap" means kinsert for sure (ask !)
  if(kinsert(proc->pgdir, &pi2, 0x0, 0) != 0){
    cprintf("FAIL : couldn't call kinsert again for p2(2)\n");
    return false;
  }
  //pte_p2 PTE_W should be cleared now
  if(*pte_p2 & PTE_W != 0){
    cprintf("FAIL : pte_p2 PTE_W bit still set.\n");
    return false;
  }
  cprintf("SUCCESS : Could change permissions – pte_p2 PTE_W bit was cleared.\n");

	//Try to remap at a place where kinsert will fail because it will need to allocate another page table.  
  if(kinsert(proc->pgdir, &pi2, 0x400000, 0) == 0){
    cprintf("FAIL : was able to kinsert, shouldn't have been able to allocate one more page table for it.\n");
    return false;
  }
  cprintf("SUCCESS : kinsert failed as expected (not able to allocate a new page table).\n");

	//Insert a different page, e.g. p2 at 0x1000.
  if(kinsert(proc->pgdir, &pi2, 0x1000, 0) != 0){
    cprintf("FAIL : Couldn't insert p2 at 0x1000 (where p3 was)\n.");
    return false;
  }
  cprintf("SUCCESS : Was able to insert p2 at 0x1000.\n");

	//Check that physical page p2 is mapped in two places and verify that the reference count for p2 is 2 and for p3 is 0
  pte_t* pt = (pte_t*)p1;
  //With the addresses above, the first 2 entries of the page table store in p1 should be p2's pte
  if(pt[0] != pte_p2 || pt[1] != pte_p2){
    cprintf("FAIL : p2 isn't mapped in the first two entries of the page table in p1.\n");
    return false;
  }
  cprintf("SUCCESS : p2 is mapped twice now.\n");

  if(pi2.refcount != 2 || pi3.refcount != 0){
    cprintf("FAIL : either p2 refcount was %d instead of 2 or p3 refcount was %d instead of 0.\n", pi2.refcount, pi3.refcount);
    return false;
  }
  cprintf("SUCCESS : p2's refcount was 2 and p3's refcount was 0, as expected.\n");

	//Assert that kalloc returns p3.
  if(kalloc() != p3){
    cprintf("FAIL : kalloc() doesn't return p3 as it should be.\n");
    return false;
  }
  cprintf("SUCCESS : kalloc() returns p3 as expected.\n");

	//kremove the reference to p2 at 0x0. Assert p2 is still mapped at 0x1000.
  kremove(proc->pgdir, 0x0);
  if(pt[1] != pte_p2){
    cprintf("FAIL : removing ref to p2 at 0x0 seems to have removed it from 0x1000 too.\n");
    return false;
  }
  cprintf("SUCCESS : p2 is still mapped at 0x1000.\n");

	//Assert that the reference count to p2 has been decremented to 1.
  if(pi2.refcount != 1){
    cprintf("FAIL : p2's refcount is %d instead of 1\n", pi2.refcount);
    return false;
  }
  cprintf("SUCCESS : p2's refcount is 1, as expected.\n");

	//Reinsert p2 at 0x1000 and assert that the reference count is still 1.
  if(kinsert(proc->pgdir, &pi2, 0x1000, 0) != 0){
    cprintf("FAIL : couldn't re insert p2 at 0x1000.\n");
    return false;
  }
  if(pi2.refcount != 1){
    cprintf("FAIL : reinserting p2 at 0x1000 changed its refcount from 1 to %d\n", pi2.refcount);
    return false;
  }
  cprintf("SUCESS : reinserting p2 at 0x1000 didn't change its refcount, still is 1.\n");

	//Remove the mapping of p2, verify that it is freed.  Assert that when you kallocate you get it back.
  kremove(proc->pgdir, 0x1000);
  if(pi2.used != 0 || pi2.refcount != 0){
    cprintf("FAIL : p2 wasn't freed correctly. refcount is %d and used field is %d.\n", pi2.refcount, pi2.used);
    return false;
  }
  if((ptr = kalloc()) != p2){
    cprintf("FAIL : Getting %p instead of p2 (%p) when calling kalloc() again.\n", ptr, p2);
    return false;
  }
  cprintf("SUCCESS : p2 was freed correctly and calling kalloc() gets it back.\n");

	//Free the pages and restore the free list.
  kdecref(p1);
  kdecref(p2);
  kdecref(p3);

  if(pi1.refcount != 0 || pi2.refcount != 0 || pi3.refcount != 0){ //Sanity check, make sure they were freed correctly
    cprintf("FAIL : Didn't free properly.\n");
    return false;
  }

  kmem.freelist = t_freelist;

	return true;
}


// Bootstrap processor starts running C code here.
// Allocate a real stack and switch to it, first
// doing some setup required for memory allocator to work.
int
main(void)
{
  kinit1(end, P2V(4*1024*1024)); // phys page allocator
  kvmalloc();      // kernel page table
  uartinit();      // serial port
          bool success;
	//bool success = test_page_free_list();
	//success ? uartprintcstr("Test_page_free_list succeeded!\n") : uartprintcstr("Test_page_free_list failed!\n");
  mpinit();        // detect other processors
  lapicinit();     // interrupt controller
  seginit();       // segment descriptors
  cprintf("\ncpu%d: starting xv6\n\n", cpunum());
  picinit();       // another interrupt controller
  ioapicinit();    // another interrupt controller
  consoleinit();   // console hardware
  uartinit();      // serial port (Have to call it twice to get interrupt output)

  cprintf("6828 decimal is %o octal!\n", 6828);
  
  pinit();         // process table
  tvinit();        // trap vectors
  binit();         // buffer cache
  fileinit();      // file table
  ideinit();       // disk
  if(!ismp)
    timerinit();   // uniprocessor timer
  startothers();   // start other processors
  kinit2(P2V(4*1024*1024), P2V(PHYSTOP)); // must come after startothers()
  kmarkused((char*)KERNBASE, end);
  kmarkused(P2V(PHYSTOP), (char*)0xFFFFFFFF);

//	success = test_page_free_list_ext();
//	success ? uartprintcstr("Test_page_free_list_ext succeded!\n") : uartprintcstr("Test_page_free_list_ext failed!\n");

	success = test_page_alloc();
	success ? uartprintcstr("Test_page_alloc succeeded!\n") : uartprintcstr("Test_page_alloc failed!\n");

	success = test_page();
	success ? uartprintcstr("Test_page succeeded!\n") : uartprintcstr("Test_page failed!\n");

  userinit();      // first user process
  mpmain();        // finish this processor's setup
}

// Other CPUs jump here from entryother.S.
static void
mpenter(void)
{
  switchkvm();
  seginit();
  lapicinit();
  mpmain();
}

// Common CPU setup code.
static void
mpmain(void)
{
  cprintf("cpu%d: starting\n", cpunum());
  idtinit();       // load idt register
  xchg(&cpu->started, 1); // tell startothers() we're up
#if LAB == 1
// Test the stack back trace
  test_backtrace(5);

  while (1)
    monitor(0);
#else
   scheduler();     // start running processes
#endif
}

pde_t entrypgdir[];  // For entry.S

// Start the non-boot (AP) processors.
static void
startothers(void)
{
  extern uchar _binary_entryother_start[], _binary_entryother_size[];
  uchar *code;
  struct cpu *c;
  char *stack;

  // Write entry code to unused memory at 0x7000.
  // The linker has placed the image of entryother.S in
  // _binary_entryother_start.
  code = P2V(0x7000);
  memmove(code, _binary_entryother_start, (uint)_binary_entryother_size);

  for(c = cpus; c < cpus+ncpu; c++){
    if(c == cpus+cpunum())  // We've started already.
      continue;

    // Tell entryother.S what stack to use, where to enter, and what
    // pgdir to use. We cannot use kpgdir yet, because the AP processor
    // is running in low  memory, so we use entrypgdir for the APs too.
    stack = kalloc();
    increfcount(stack);
    *(void**)(code-4) = stack + KSTACKSIZE;
    *(void**)(code-8) = mpenter;
    *(int**)(code-12) = (void *) V2P(entrypgdir);

    lapicstartap(c->apicid, V2P(code));

    // wait for cpu to finish mpmain()
    while(c->started == 0)
      ;
  }
}

// The boot page table used in entry.S and entryother.S.
// Page directories (and page tables) must start on page boundaries,
// hence the __aligned__ attribute.
// PTE_PS in a page directory entry enables 4Mbyte pages.

__attribute__((__aligned__(PGSIZE)))
pde_t entrypgdir[NPDENTRIES] = {
  // Map VA's [0, 4MB) to PA's [0, 4MB)
  [0] = (0) | PTE_P | PTE_W | PTE_PS,
  // Map VA's [KERNBASE, KERNBASE+4MB) to PA's [0, 4MB)
  [KERNBASE>>PDXSHIFT] = (0) | PTE_P | PTE_W | PTE_PS,
};

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
