// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.
extern struct cpu cpus[NCPU];
char* kmem_names[NCPU] = {"kmem0", "kmem1", "kmem2", "kmem3", "kmem4", "kmem5", "kmem6", "kmem7"};
struct run {
  struct run *next;
};
void countChunks(int pp) {
  int i;
  struct run* ptr = cpus[pp].freelist;
  for(i=0;ptr!=0;i++,ptr = ptr->next);
  printf("count :: cpu: %d mem:%d\n",pp,i);
}
void
kinit()
{
  for (int i=0;i<NCPU;i++)
    cpus[i].freelist = 0, initlock(&cpus[i].lock, kmem_names[i]);
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  acquire(&cpus[0].lock);
  for(struct run *r; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    r = (struct run*)p;
    r->next = cpus[0].freelist;
    cpus[0].freelist = r;
  }
  release(&cpus[0].lock);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int i = cpuid();
  acquire(&cpus[i].lock);
  r->next = cpus[i].freelist;
  cpus[i].freelist = r;
  release(&cpus[i].lock);
  pop_off();
}

void* steal_page(int i) {
  // interrupt should be disabled
  void *ret = 0;
  for (int j=((i+1)%NCPU); !ret && j!=i; j=((j+1)%NCPU)) {
    acquire(&cpus[j].lock);
    if (cpus[j].freelist) {
      ret = cpus[j].freelist;
      cpus[j].freelist = cpus[j].freelist->next;
    }
    release(&cpus[j].lock);
  }
  return ret;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int i = cpuid();
  acquire(&cpus[i].lock);
  if (!cpus[i].freelist) {
    r = steal_page(i);
  } else {
    r = cpus[i].freelist;
    cpus[i].freelist = r->next;
  }

  release(&cpus[i].lock);
  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
