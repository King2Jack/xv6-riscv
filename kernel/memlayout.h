// Physical memory layout

// qemu -machine virt is set up like this,
// based on qemu's hw/riscv/virt.c:
//
// 00001000 -- boot ROM, provided by qemu
// 02000000 -- CLINT
// 0C000000 -- PLIC
// 10000000 -- uart0 
// 10001000 -- virtio disk 
// 80000000 -- boot ROM jumps here in machine mode
//             -kernel loads the kernel here
// unused RAM after 80000000.

// the kernel uses physical memory thus:
// 80000000 -- entry.S, then kernel text and data
// end -- start of kernel page allocation area
// PHYSTOP -- end RAM used by the kernel

// qemu puts UART registers here in physical memory.
#define UART0 0x10000000L
#define UART0_IRQ 10

// virtio mmio interface
#define VIRTIO0 0x10001000
#define VIRTIO0_IRQ 1

// core local interruptor (CLINT), which contains the timer.
#define CLINT 0x2000000L
#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8*(hartid))
#define CLINT_MTIME (CLINT + 0xBFF8) // cycles since boot.

// qemu puts platform-level interrupt controller (PLIC) here.
#define PLIC 0x0c000000L
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_MENABLE(hart) (PLIC + 0x2000 + (hart)*0x100)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_MPRIORITY(hart) (PLIC + 0x200000 + (hart)*0x2000)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_MCLAIM(hart) (PLIC + 0x200004 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)

// the kernel expects there to be RAM
// for use by the kernel and user pages
// from physical address 0x80000000 to PHYSTOP.
#define KERNBASE 0x80000000L
#define PHYSTOP (KERNBASE + 128*1024*1024)

// map the trampoline page to the highest address,
// in both user and kernel space.
/**
 * Trampoline 是一个特殊的页面，它包含了一个跳板函数（通常是uservec），用于在用户态代码和内核态代码之间进行切换。
 * 当一个进程在用户态执行时遇到一个系统调用或异常（如除零错误、页错误等），CPU会自动保存当前的上下文到trapframe，
 * 然后跳转到trampoline页面中预先设定的地址继续执行。trampoline页面中的代码负责从用户态切换到内核态，处理完系统调用或异常后，
 * 再从内核态安全地回到用户态。
 * Trampoline页面在每个进程的虚拟地址空间中都存在，通常映射到虚拟地址空间的最高地址处。
 * 它没有PTE_U标志，这意味着它只能在内核态访问，不能在用户态直接执行。
 */
#define TRAMPOLINE (MAXVA - PGSIZE)

// map kernel stacks beneath the trampoline,
// each surrounded by invalid guard pages.
#define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*PGSIZE)

// User memory layout.
// Address zero first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
//   ...
//   TRAPFRAME (p->trapframe, used by the trampoline)
//   TRAMPOLINE (the same page as in the kernel)
/**
 * Trapframe 是一个结构体，用于保存发生系统调用或异常时的CPU寄存器状态和上下文信息。当一个系统调用或异常发生时，
 * CPU会自动保存当前的寄存器状态到trapframe中，以便在异常处理完成后能够恢复到发生异常前的执行状态。
 * 在xv6中，trapframe通常被分配在每个进程的虚拟地址空间的顶部，紧邻trampoline页面之下。当一个进程被创建时，
 * xv6会为该进程分配一个trapframe页面，并将其映射到固定的虚拟地址（如TRAPFRAME），这样无论哪个进程发生异常，
 * 都可以使用相同的虚拟地址访问其trapframe。
 * Trapframe的结构体通常包含以下信息：
 * CPU寄存器的值（如eax, ebx, ecx, edx, esp, ebp, eip等，对于RISC-V架构则对应于x1至x31和pc等寄存器）。
 * 状态标志（如eflags，对于RISC-V则是status和cause寄存器）。
 * 在处理完系统调用或异常后，内核会从trapframe中恢复CPU寄存器的值，使得进程能够从引发异常的指令处继续执行。
 * 总的来说，trampoline和trapframe共同协作，使得xv6能够在用户态和内核态之间安全、高效地切换执行上下文。
 */
#define TRAPFRAME (TRAMPOLINE - PGSIZE)
