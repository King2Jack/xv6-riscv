#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}

void
ls(char *path)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  // 以只读方式打开文件
  if((fd = open(path, 0)) < 0){
    // 打开失败
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  // 打开文件的状态信息
  if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  // 根据不同类型做不同操作
  switch(st.type){
      // 普通文件或者是设备文件
  case T_DEVICE:
  case T_FILE:
    // 打印格式化后的文件路径,文件名,inode,文件大小
    printf("%s %d %d %l\n", fmtname(path), st.type, st.ino, st.size);
    break;

  case T_DIR:
    // 目录
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      // 路径长度超出缓冲区大小
      printf("ls: path too long\n");
      break;
    }
    // 将path复制到buf
    strcpy(buf, path);
    p = buf+strlen(buf);
    // 添加/
    *p++ = '/';
    // 循环读取目录项信息到buf中,直到失败或者文件末尾
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      // inode为0,直接跳过
      if(de.inum == 0)
        continue;
      //
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf("ls: cannot stat %s\n", buf);
        continue;
      }
      printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
    }
    break;
  }
  // 关闭文件描述符
  close(fd);
}

int
main(int argc, char *argv[])
{
  int i;

  if(argc < 2){
    ls(".");
    exit(0);
  }
  for(i=1; i<argc; i++)
    ls(argv[i]);
  exit(0);
}
