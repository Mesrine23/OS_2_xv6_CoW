// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

int ref_count[PHYSTOP/PGSIZE]; //reference counter

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
  {
    ref_count[(uint64)p/PGSIZE] = 0;
    kfree(p);
  }
}

void 
increase_ref_counter(uint64 p)
{
  //printf("increase\n");
  acquire(&kmem.lock);  //get the lock
  int index = p/PGSIZE; //get the index for referece counter
  if(p>=PHYSTOP || ref_count[index]<1)
    panic("increase: problem with ref_counter");
  ref_count[index]++;   //increase reference counter by 1
  release(&kmem.lock);  //release the lock
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  r = (struct run *)pa;
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&kmem.lock);
  if (ref_count[(uint64)r / PGSIZE] > 0)
    ref_count[(uint64)r / PGSIZE]--;
  int check = ref_count[(uint64)r / PGSIZE];
  release(&kmem.lock);

  if(check>0)
    return;
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;  

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
  { 
    ref_count[(uint64)r/PGSIZE] = 1;
    kmem.freelist = r->next;
  }
  release(&kmem.lock);
  

  if(r)
  {
    //ref_count[(uint64)r/PGSIZE] = 1;
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
  return (void*)r;
}
