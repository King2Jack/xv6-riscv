//
// low-level driver routines for 16550a UART.
//

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

// the UART control registers are memory-mapped
// at address UART0. this macro returns the
// address of one of the registers.
#define Reg(reg) ((volatile unsigned char *)(UART0 + reg))

// the UART control registers.
// some have different meanings for
// read vs write.
// see http://byterunner.com/16550.html
#define RHR 0                 // receive holding register (for input bytes)
#define THR 0                 // transmit holding register (for output bytes)
#define IER 1                 // interrupt enable register
#define IER_RX_ENABLE (1<<0)
#define IER_TX_ENABLE (1<<1)
#define FCR 2                 // FIFO control register
#define FCR_FIFO_ENABLE (1<<0)
#define FCR_FIFO_CLEAR (3<<1) // clear the content of the two FIFOs
#define ISR 2                 // interrupt status register
#define LCR 3                 // line control register
#define LCR_EIGHT_BITS (3<<0)
#define LCR_BAUD_LATCH (1<<7) // special mode to set baud rate
#define LSR 5                 // line status register
#define LSR_RX_READY (1<<0)   // input is waiting to be read from RHR
#define LSR_TX_IDLE (1<<5)    // THR can accept another character to send

#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

// the transmit output buffer.
struct spinlock uart_tx_lock;
#define UART_TX_BUF_SIZE 32
char uart_tx_buf[UART_TX_BUF_SIZE];
uint64 uart_tx_w; // write next to uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE]
uint64 uart_tx_r; // read next from uart_tx_buf[uart_tx_r % UART_TX_BUF_SIZE]

extern volatile int panicked; // from printf.c

void uartstart();

void
uartinit(void)
{
  // disable interrupts.
  // 禁用UART相关所有中断
  WriteReg(IER, 0x00);

  // special mode to set baud rate.
  // 将线路控制寄存器 (LCR) 设置到波特率寄存器可访问模式。
  WriteReg(LCR, LCR_BAUD_LATCH);

  // LSB for baud rate of 38.4K.
  WriteReg(0, 0x03);

  // MSB for baud rate of 38.4K.
  WriteReg(1, 0x00);

  // leave set-baud mode,
  // and set word length to 8 bits, no parity.
  // 将 LCR 寄存器设置为8位数据长度，并禁用奇偶校验，这表示每个数据帧将包含8位数据，无奇偶校验位
  WriteReg(LCR, LCR_EIGHT_BITS);

  // reset and enable FIFOs.
  // 初始化FIFO（First In First Out）缓冲区，使其处于启用状态并清除其内容。
  WriteReg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);

  // enable transmit and receive interrupts.
  // 开启UART的发送和接收中断，以便处理数据的发送和接收。
  WriteReg(IER, IER_TX_ENABLE | IER_RX_ENABLE);

  // 初始化一个互斥锁，通常用于同步多个线程或进程在发送数据时对UART的访问，避免冲突。
  initlock(&uart_tx_lock, "uart");
}

// add a character to the output buffer and tell the
// UART to start sending if it isn't already.
// blocks if the output buffer is full.
// because it may block, it can't be called
// from interrupts; it's only suitable for use
// by write().
void
uartputc(int c)
{
  acquire(&uart_tx_lock);
  if(panicked){
    for(;;)
      ;
  }
  // 判断缓冲区是否已满,也就是写指针=读指针+缓冲区大小
  while(uart_tx_w == uart_tx_r + UART_TX_BUF_SIZE){
    // buffer is full.
    // wait for uartstart() to open up space in the buffer.
    // 函数将调用`sleep(&uart_tx_r, &uart_tx_lock)`使当前进程进入睡眠状态，
    // 等待其他进程通过`uartstart()`函数发送数据并更新`uart_tx_r`，从而腾出缓冲区空间
    sleep(&uart_tx_r, &uart_tx_lock);
  }
  // 写入字符
  uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE] = c;
  // 更新写指针
  uart_tx_w += 1;
  // 启动发送,调用uartstart()函数,通知硬件开始发送缓冲区中的数据
  uartstart();
  // 释放锁
  release(&uart_tx_lock);
}


// alternate version of uartputc() that doesn't 
// use interrupts, for use by kernel printf() and
// to echo characters. it spins waiting for the uart's
// output register to be empty.
void
uartputc_sync(int c)
{
  // 关闭中断
  push_off();
  // 判断进程是否崩溃
  if(panicked){
    for(;;)
      ;
  }

  // wait for Transmit Holding Empty to be set in LSR.
  while((ReadReg(LSR) & LSR_TX_IDLE) == 0)
    ;
  WriteReg(THR, c);
  // 恢复中断
  pop_off();
}

// if the UART is idle, and a character is waiting
// in the transmit buffer, send it.
// caller must hold uart_tx_lock.
// called from both the top- and bottom-half.
void
uartstart()
{
  while(1){
    // 检查uart发送缓冲区是否为空
    if(uart_tx_w == uart_tx_r){
      // transmit buffer is empty.
      return;
    }
    /*
     * 读取LSR寄存器,检查LSR_TX_IDLE标志位
     * 如果LSR_TX_IDLE标志位为0,则表示UART的传输寄存器已满,不能再写入数据
     */
    if((ReadReg(LSR) & LSR_TX_IDLE) == 0){
      // the UART transmit holding register is full,
      // so we cannot give it another byte.
      // it will interrupt when it's ready for a new byte.
      return;
    }

    /*
     * 从发送缓冲区当中读取数据
     * 并且更新发送缓冲区的读指针
     */
    int c = uart_tx_buf[uart_tx_r % UART_TX_BUF_SIZE];
    uart_tx_r += 1;
    
    // maybe uartputc() is waiting for space in the buffer.
    // 唤醒可能在等待发送缓冲区空间的进程
    wakeup(&uart_tx_r);
    // 将字符写入UART的传输寄存器
    WriteReg(THR, c);
  }
}

// read one input character from the UART.
// return -1 if none is waiting.
int
uartgetc(void)
{
  if(ReadReg(LSR) & 0x01){
    // input data is ready.
    return ReadReg(RHR);
  } else {
    return -1;
  }
}

/**
 * 处理uart中断的函数,由devintr函数调用
 */
// handle a uart interrupt, raised because input has
// arrived, or the uart is ready for more output, or
// both. called from devintr().
void
uartintr(void)
{
  // read and process incoming characters.
  // 通过无限循环,尝试从uart接收端读取字符
  while(1){
    // 读取一个字符
    int c = uartgetc();
    if(c == -1)
      // 没有字符可读,则退出
      break;
    // 对于读取到的字符,调用consoleintr函数进行处理,通常包括字符的控制台回显(consputc函数)和别的处理逻辑(consoleread函数)
    consoleintr(c);
  }

  // send buffered characters.
  acquire(&uart_tx_lock);
  // 为什么要在一个接收读取字符里调用发送缓冲区字符????
  // 发送缓冲区的字符,启动UART设备发送缓冲区中的字符。这通常意味着将缓冲区中的数据串行化并通过UART接口发送出去。
  uartstart();
  release(&uart_tx_lock);
}
