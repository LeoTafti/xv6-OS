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
    increfcount(realloc_pages[i]);
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

	//Assert that they are all non-zero and different from one another

	//Save the free page list to a temporary variable.  Set free page list to zero.

	//Assert that kookup(0x0) == 0

	//Assert that you can not allocate a page table with kinsert

	//Free page p1, kinsert the physical page p2 at 0x0. Assert the operation succeeded.

	//Assert that p1 is the page table from the previous step.  Assert p2 is in that page table.

	//Asset that p1 and p2 have a ref count of 1.

	//Kinsert p3 at 0x1000.

	//Assert that p3 is also in the page table stored at p1.  Assert p3 has a ref count of 1.

	//Reinsert p3 at 0x1000.

	//Assert that p3 is still in the page table stored at p1.  Assert p3 still has a reference count of 1.

	//Assert you can no longer allocate any more pages.

	//Change the permissions on the pages with kinsert. Assert permissions were changed correctly.

	//Do a remap with fewer permissions on the pages with kinsert.  Assert permissions were changed correctly.

	//Try to remap at a place where kinsert will fail because it will need to allocate another page table.  

	//Insert a different page, e.g. p2 at 0x1000.

	//Check that physical page p2 is mapped in two places and verify that the reference count for p2 is 2 and for p3 is 0

	//Assert that kalloc returns p3.

	//kremove the reference to p2 at 0x0. Assert p2 is still mapped at 0x1000.

	//Assert that the reference count to p2 has been decremented to 1.

	//Reinsert p2 at 0x1000 and assert that the reference count is still 1.

	//Remove the mapping of p2, verify that it is freed.  Assert that when you kallocate you get it back.

	//Free the pages and restore the free list.

	return 0;
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
