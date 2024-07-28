//
// Created by KingJack on 2024/7/23.
//

#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    char buf = 'p';
    int p2c[2], c2p[2];
    if (pipe(p2c) < 0 || pipe(c2p) < 0) {
        fprintf(2, "pipe failed\n");
        exit(1);
    }

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    // 子进程
    if (pid == 0) {
        // 子进程需要从父进程管道中读取数据,往子进程管道中写数据
        // 所以可以关闭父进程管道的写端,关闭子进程管道的读端
        close(p2c[1]);
        close(c2p[0]);
        // 读取父进程管道数据
        if (read(p2c[0], &buf, 1) == -1) {
            fprintf(2, "read failed\n");
            close(p2c[0]);
            close(c2p[1]);
            exit(1);
        }
        printf("%d: received ping\n", getpid());
        // 写入子进程管道数据
        if (write(c2p[1], &buf, 1) == -1) {
            fprintf(2, "write failed\n");
            close(p2c[0]);
            close(c2p[1]);
            exit(1);
        }
        close(p2c[0]);
        close(c2p[1]);
        exit(0);
    } else {
        // 父进程
        // 父进程需要往父进程管道写数据,往子进程管道读数据
        // 所以可以关闭父进程管道的读端,关闭子进程管道的写端
        close(p2c[0]);
        close(c2p[1]);
        // 写入父进程管道数据
        if (write(p2c[1], &buf, 1) == -1) {
            fprintf(2, "write failed\n");
            close(p2c[1]);
            close(c2p[0]);
            exit(1);
        }
        // 读取子进程管道数据
        if (read(c2p[0], &buf, 1) == -1) {
            fprintf(2, "read failed\n");
            close(p2c[1]);
            close(c2p[0]);
            exit(1);
        }
        printf("%d: received pong\n", getpid());
        close(p2c[1]);
        close(c2p[0]);
        exit(0);
    }
}