// init: The initial user-level program

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "user/user.h"
#include "kernel/fcntl.h"

char *argv[] = { "sh", 0 };

int
main(void)
{
  int pid, wpid;

  // 尝试打开console的设备文件,并且使用读写模式
  // 如果打开失败,则创建一个console设备文件,并且再次尝试打开
  if(open("console", O_RDWR) < 0){
    mknod("console", CONSOLE, 0);
    open("console", O_RDWR);
  }
  // 使用 `dup(0)` 函数两次，将标准输入文件描述符（通常为0）复制给标准输出和标准错误，确保所有输出和错误信息都导向 "console" 设备。
  dup(0);  // stdout
  dup(0);  // stderr

  for(;;){
    printf("init: starting sh\n");
    // 创建一个子进程
    pid = fork();
    if(pid < 0){
      // 子进程创建失败,退出
      printf("init: fork failed\n");
      exit(1);
    }
    if(pid == 0){
      // 调用exec系统调用,替换当前进程映像为shell,并传入参数
      exec("sh", argv);
      // 执行失败,输出错误信息,并且退出
      printf("init: exec sh failed\n");
      exit(1);
    }

    // 父进程中,进入循环等待子进程结束
    for(;;){
      // this call to wait() returns if the shell exits,
      // or if a parentless process exits.
      // 使用wait函数阻塞直到子进程结束或者发生错误
      wpid = wait((int *) 0);
      if(wpid == pid){
        // 如果返回的进程ID等于shell的PID,表示shell已结束,跳出循环,重新创建shell进程
        // the shell exited; restart it.
        break;
      } else if(wpid < 0){
        // 出现错误,输出错误信息退出
        printf("init: wait returned an error\n");
        exit(1);
      } else {
        // 表示有无父进程的子进程结束,不做任何处理
        // it was a parentless process; do nothing.
      }
    }
  }
}
