//
// Console input and output, to the uart.
// Reads are line at a time.
// Implements special input characters:
//   newline -- end of line
//   control-h -- backspace
//   control-u -- kill line
//   control-d -- end of file
//   control-p -- print process list
//

#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

#define BACKSPACE 0x100
#define C(x)  ((x)-'@')  // Control-x

/**
 * 向uart发送一个字符
 * @param c
 */
//
// send one character to the uart.
// called by printf(), and to echo input characters,
// but not from write().
//
void
consputc(int c)
{
  // 判断输入字符是否为退格键,是的话则转为发送一个退格键+空格+退格,用来实现一个删除的效果
  if(c == BACKSPACE){
    // if the user typed backspace, overwrite with a space.
    uartputc_sync('\b'); uartputc_sync(' '); uartputc_sync('\b');
  } else {
    // 直接向uart发送该字符
    uartputc_sync(c);
  }
}

struct {
  struct spinlock lock;
  
  // input
#define INPUT_BUF_SIZE 128
  char buf[INPUT_BUF_SIZE];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} cons;

//
// user write()s to the console go here.
//
int
consolewrite(int user_src, uint64 src, int n)
{
  int i;

  for(i = 0; i < n; i++){
    char c;
    if(either_copyin(&c, user_src, src+i, 1) == -1)
      break;
    uartputc(c);
  }

  return i;
}

//
// user read()s from the console go here.
// copy (up to) a whole input line to dst.
// user_dist indicates whether dst is a user
// or kernel address.
//
int
consoleread(int user_dst, uint64 dst, int n)
{
  uint target;
  int c;
  char cbuf;

  target = n;
  // 获取控制台锁,确保线程安全
  acquire(&cons.lock);
  // 当需要读取的字符数大于0
  while(n > 0){
    // wait until interrupt handler has put some
    // input into cons.buffer.
    // 等待输入,直到有内容填充进cons.buffer当中
    // 如果一直没有内容,则都通过sleep等待
    while(cons.r == cons.w){
      // 检查进程是否已经被终止
      if(killed(myproc())){
        release(&cons.lock);
        return -1;
      }
      sleep(&cons.r, &cons.lock);
    }

    // 从输入缓冲区当中读取第一个字符
    c = cons.buf[cons.r++ % INPUT_BUF_SIZE];

    // 如果第一个字符是文件结束符D
    if(c == C('D')){  // end-of-file
      // 若已读取的字节数小于目标读取数，则保留文件结束符以供下次读取。否则，直接退出循环。
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        cons.r--;
      }
      break;
    }

    // copy the input byte to the user-space buffer.
    // 复制输入的字节到用户空间缓存
    cbuf = c;
    if(either_copyout(user_dst, dst, &cbuf, 1) == -1)
      break;

    // 更新目标地址
    dst++;
    // 剩余读取字节数
    --n;

    // 读取到换行符,默认一行数据已完整读入,退出循环
    if(c == '\n'){
      // a whole line has arrived, return to
      // the user-level read().
      break;
    }
  }
  // 释放锁
  release(&cons.lock);
  // 返回实际读取的字节数
  return target - n;
}

/**
 * 控制台输入中断处理函数
 * 在输入字符时被uartintr调用
 * @param c
 */
//
// the console input interrupt handler.
// uartintr() calls this for input character.
// do erase/kill processing, append to cons.buf,
// wake up consoleread() if a whole line has arrived.
//
void
consoleintr(int c)
{
  // 先获取锁
  acquire(&cons.lock);

  // 判断输入的字符
  switch(c){
  // 打印进程列表
  case C('P'):  // Print process list.
    procdump();
    break;
  // 清除当前行
  case C('U'):  // Kill line.
    while(cons.e != cons.w &&
          cons.buf[(cons.e-1) % INPUT_BUF_SIZE] != '\n'){
      cons.e--;
      consputc(BACKSPACE);
    }
    break;
  // 输入退格键或者删除键,如果缓存非空,则删除当前字符并且退出
  case C('H'): // Backspace
  case '\x7f': // Delete key
    if(cons.e != cons.w){
      cons.e--;
      consputc(BACKSPACE);
    }
    break;
  default:
    if(c != 0 && cons.e-cons.r < INPUT_BUF_SIZE){
      // 如果是回车符,则将回车符替换为换行符
      c = (c == '\r') ? '\n' : c;

      // echo back to the user.
      // 字符回显到控制台
      consputc(c);

      // store for consumption by consoleread().
      // 将字符缓存到缓冲区cons.buf当中,用于consoleread函数消费
      cons.buf[cons.e++ % INPUT_BUF_SIZE] = c;

      // 如果字符是换行符或者结束输入或者是缓存已满,则唤醒consoleread函数
      // 说白了就是要读取完整的一行
      if(c == '\n' || c == C('D') || cons.e-cons.r == INPUT_BUF_SIZE){
        // wake up consoleread() if a whole line (or end-of-file)
        // has arrived.
        cons.w = cons.e;
        wakeup(&cons.r);
      }
    }
    break;
  }
  // 释放控制台锁
  release(&cons.lock);
}

void
consoleinit(void)
{
  // 先获得锁
  initlock(&cons.lock, "cons");
  // 初始化uart
  uartinit();

  // connect read and write system calls
  // to consoleread and consolewrite.
  // 更新设备驱动程序表中对应控制台设备的操作函数
  // 控制台读取操作设置为consoleread
  devsw[CONSOLE].read = consoleread;
  // 控制台写入操作设置为consolewrite
  devsw[CONSOLE].write = consolewrite;
}
