#include "syscall.h"
#include "keyboard.h"
#include "process.h"
#include "vga.h"
void process_exit();

void syscall_handler(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
  switch (eax) {
  case SYS_WRITE:
    if (ebx == STDOUT && edx > 0) {
      char *buf = (char *)ecx;
      for (int i = 0; i < edx; i++) {
        char str[2] = {buf[i], 0};
        print(str, 0x0F);
      }
    } else if (ebx == STDERR && edx > 0) {
      char *buf = (char *)ecx;
      for (int i = 0; i < edx; i++) {
        char str[2] = {buf[i], 0};
        print(str, 0x04);
      }
    }
    break;
  case SYS_EXIT:
    print("task exit.\n", 0);
    process_exit();
    break;
  default:
    break;
  }
}
