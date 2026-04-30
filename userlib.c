
#include <stdint.h>
// 用户态系统调用库
int sys_write(int fd, const char *buf, int len) {
  int ret;
  asm volatile("int $0x80" : "=a"(ret) : "a"(1), "b"(fd), "c"(buf), "d"(len));
  return ret;
}

void sys_exit() {
  asm volatile("int $0x80" ::"a"(2));
  while (1)
    ;
}

void sys_sleep(int ticks) { asm volatile("int $0x80" ::"a"(3), "b"(ticks)); }

int sys_fork() {
  int ret;
  asm volatile("int $0x80" : "=a"(ret) : "a"(4));
  return ret;
}
int sys_exec(uint32_t start, uint32_t size) {
  int ret;
  asm volatile("int $0x80" : "=a"(ret) : "a"(5), "b"(start), "c"(size));
  return ret;
}
int sys_wait() {
  int ret;
  asm volatile("int $0x80" : "=a"(ret) : "a"(6));
  return ret;
}
extern char user_c_start;
extern char user_c_end;
void user_main() {
  volatile char msg[] = "Hello from user!\n";
  sys_write(1, (const char *)msg, 17);
  int pid = sys_fork();
  if (pid == 0) {
    volatile char child_msg[] = "Hello from child!\n";
    sys_write(1, (const char *)child_msg, 18);
    sys_exit();
  } else {
    int w;
    do { w = sys_wait(); } while (w == -1);
    volatile char done_msg[] = "Parent: child done\n";
    sys_write(1, (const char *)done_msg, 19);
    sys_exit();
  }
}

__asm__(".global user_c_end\nuser_c_end:");