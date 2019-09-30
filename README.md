# Lab 2 : Page Table Management
## Part 0 : Background
1. I would say that `x` should be a `char *` since it is also the type of `value` and we do the assignment `x = value;`

## Part 1 : Page Management
### Exercise 1
#### Question 1
##### Replacing `struct run` with `struct page_info`
1. `kalloc.h` : Added a new header file which properly defines `struct page_info` and `struct kmem`. *Note : I gave kmem's struct a name in order to be able to easily access it (and its `freelist` field in particular) from main.c, for testing purposes.*

2. `kalloc.c`, `kalloc.h` : Creating the array of `struct page_info` entries.  
	* Need to cover the address space up to PHYSTOP with 4kB pages &rarr; `PHYSTOP / PGSIZE = 0xE000` pages.
	* `kalloc.h` : defined constant `PPNB` for `0xE000`.
	* `kalloc.c` : created the array of `struct page_info` entries.

3. `kalloc.c` : Modified `kfree()` to replace `struct run` with `struct page_info` and update the `struct page_info` `next` pointer.

4. `kalloc.c` : Modified `kalloc()` to replace `struct run` with `struct page_info` and set the `struct page_info` `refcount` and `used` fields.

##### Implementing `kdecref()`
1. `kalloc.c` : Implemented `kdecref()`.

##### Marking reserved pages as used
From the calls to `kinit1` and `kinit2` in main, we see that free addresses range from `end` *("first address after kernel loaded from ELF file")* to `P2V(PHYSTOP)`.  

The pages corresponding to the remaining addresses must be marked as used. There are 2 ranges : from `KERNBASE` to `end` and from `P2V(PHYSTOP)` to `0xFFFFFFFF` (top).

1. `kalloc.c` : Added a `kmarkused()` function in `kalloc.c` which takes care of that.
2. `main.c` : Added calls to `kmarkused()` after `kinit2()` in `main()`.
3. `defs.h` : Added `kmarkused()` signature.

##### Implementing `test_page_free_list()` and extended version.
1. Here we only need to check that we respect the upper limit of 4MB, since the size of the pages we keep track of with the array of `struct page_info` is implicitely `PGSIZE`. To do so, we go through `freelist`, compute the address of each corresponding page and perform the check.
2. Instead of going through the freelist, checking for each page which should not be free that it isn't present (slow), we simply check that none of the pages kept track of in `freelist` have an address under `V2P(end)`. This check is performed at the same time as the first one (to avoid going through `freelist` a second time).
3. We check that the first free part of extended physical memory (from `V2P(end)` to a little bit before the `4 MB` limit has been mapped to free pages. Since `kvmalloc()` allocates pages to hold the page tables mapping the address space described by `kmap`, and pages on our freelist are added from low to high addresses (s.t. pages with high addresses are at this point closer to the head of the list), we stop a little bit before the `4 MB` limit. The exact number of pages allocated by `kvmalloc()` can be computed this way :

	0 to PHYSTOP = 0xE000000 => 0xE000000 bytes  
DEVSPACE = 0xFE000000 to the top  => 0x2000000 bytes  
Total : 0xfffffff bytes = 256 MB.

	Since pages have a capacity of 4 MB, we need 256 / 4 = 64 pages to hold the page tables, to which we must add 1 page to hold the page directory.

	&rArr; 65 pages allocated by `kvmalloc()`. This is why we stop at `4 MB - 65 * PGSIZE`.

	To perform the check, we iterate over the indices of the pages in memory which should be free, and going through the `freelist` each time searching for the corresponding `page_info` entry. *Although not the fastest, this solution has the advantage of being very simple. It takes a few seconds (~15s) to perform the check with in the extended version of the test.*

	Since the same check is performed in the *extended* version of the test, I defined a function `check_extmem_mapped(uint p_from, uint p_upto)` which serves this purpose (and a helper function `is_in_freelist(uint index)`).

*Note : I could not call `test_page_freelist()` from `test_page_freelist_extended()` since the first one fails for pages above 4MB. Instead, I copied and adapted the relevant checks to the extended version.*

##### Updating calls to `kalloc()` and `refcount` increment

* `kalloc.h, kalloc.c` : Defined a function `increfcount()` which takes a virtual address, finds the corresponding `page_info` and increments its `refcount` field (+ performs argument checking).
<i>
* `spinlock.h` : I added "include guards" to avoid double inclusion of the header file in files including both `spinlock.h` and `kalloc.h`</i>

#### Question 2

Not answered

### Exercise 2

* `main.c` : Implemented unit test in `test_page_alloc()`

### Exercise 3

* `kalloc.c` : Added `kinsert()`, `klookup()`, `kremove()`
* `defs.h` : Added declarations for `kinsert()`, `klookup()`, `kremove()`
* `vm.c` : Implemented `tlb_invalidate()`
* `main.c` : Implemented unit test in `test_page()`

### Exercise 4

<i>**DISCLAIMER** : I did not have the time to completely debug my code and make CoW forking work (and had to start working on Homework 4...), but I still implemented it as well as I could.</i>

* `proc.h` : Defined `PTE_COW`
* `x86.h` : Defined `ERR_W` *(found the info [here](https://pdos.csail.mit.edu/6.828/2005/lec/lec8-slides.pdf), on the last slide)*
* `trap.c` : Implemented trap handler for COW

### Exercise 5

* `vm.c` : Defined `vcopyuvm()`
* `defs.h` : Added declaration of `vcopyuvm()`
* `proc.c` : Replaced `copyuvm()` with `vcopyuvm()`
