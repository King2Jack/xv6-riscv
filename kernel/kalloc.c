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

/**
 * 初始化内核内存管理的数据结构,设置可用内存范围
 */
void
kinit()
{
  initlock(&kmem.lock, "kmem");
  // 将从end到PHYSTOP的内存作为可用内存,这个函数会将这些内存页添加到可用内存列表中
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  // 校验传进来的地址范围是否合法
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // 释放内存需要加锁
  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

/**
 * 分配一个4096字节大小的物理内存页,返回一个内核可以使用的指针
 * @return
 */
// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  // 获取锁
  acquire(&kmem.lock);
  // 从空闲列表当中获取
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  // 释放锁
  release(&kmem.lock);

  if(r)
    // 函数会调用`memset`函数将新分配的内存块填充为特定值（这里是5）,这通常是为了防止敏感信息泄露,同时也可以帮助检测未初始化的内存使用
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
