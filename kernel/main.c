#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

volatile static int started = 0;

// start() jumps here in supervisor mode on all CPUs.
void
main()
{
  if(cpuid() == 0){
    consoleinit();  // 初始化控制台
    printfinit();   // 初始化打印函数
    // 打印启动信息
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    // 初始化物理页面分配器
    kinit();         // physical page allocator
    // 创建内核页表
    kvminit();       // create kernel page table
    // 开启分页机制
    kvminithart();   // turn on paging
    // 初始化进程表
    procinit();      // process table
    // 初始化陷阱向量
    trapinit();      // trap vectors
    // 安装内核陷阱向量
    trapinithart();  // install kernel trap vector
    // 设置中断控制器
    plicinit();      // set up interrupt controller
    // 请求设备中断
    plicinithart();  // ask PLIC for device interrupts
    // 初始化缓冲区
    binit();         // buffer cache
    // 初始化inode表
    iinit();         // inode table
    // 初始化文件表
    fileinit();      // file table
    // 初始化模拟硬盘
    virtio_disk_init(); // emulated hard disk
    // 初始化第一个用户程序
    userinit();      // first user process
    // 同步变量,设置为1
    __sync_synchronize();
    started = 1;
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
  }

  scheduler();        
}
