#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#define STDIN 0
#define STDOUT 1
#define STDERR 2

#define SYS_READ  0
#define SYS_WRITE  1
#define SYS_EXIT   2

void syscall_init();
void syscall_handler(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) ;
static inline void sys_exit() {
  asm volatile("mov $2, %%eax; int $0x80" ::: "eax");
}
static inline int sys_write(int fd, const char *buf, int count) {
  int ret;
  asm volatile(
    "mov $1, %%eax\n"
    "int $0x80\n"
    : "=a"(ret)
    : "b"(fd), "c"(buf), "d"(count)
  );
  return ret;
}
#endif
