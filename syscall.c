#include "syscall.h"
#include "keyboard.h"
#include "process.h"
#include "vga.h"
#include "vmm.h"
#include <stdint.h>

int syscall_handler(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
  switch (eax) {
  case SYS_READ:
    if (ebx == STDIN && edx > 0) {
      char *buf = (char *)ecx;
      int count = 0;
      while (count < edx && kb_head != kb_tail) {
        buf[count] = kb_getchar();
        count++;
        if (buf[count - 1] == '\n') {
          break;
        }
      }
      return count;
    }
    break;
  case SYS_WRITE:
    if (ebx == STDOUT && edx > 0) {
      char *buf = (char *)ecx;
      for (int i = 0; i < edx; i++) {
        char str[2] = {buf[i], 0};
        print(str, 0x0F);
      }
      return edx;
    } else if (ebx == STDERR && edx > 0) {
      char *buf = (char *)ecx;
      for (int i = 0; i < edx; i++) {
        char str[2] = {buf[i], 0};
        print(str, 0x04);
      }
      return edx;
    }
    break;
  case SYS_EXIT:
    print("task exit.\n", 0);
    process_exit();
    break;
  case SYS_FORK:
    return process_fork(0);
  case SYS_SLEEP:
    if (ebx > 0)
      process_sleep(ebx);
    break;
  case SYS_EXEC: {
    uint32_t start = ebx; // 起始地址
    uint32_t size = ecx;  // 大小
    process_t *p = &processes[current];
    // 先分配新页
    uint32_t code_size = size;
    if (code_size > 4096)
      code_size = 4096;
    uint32_t code_page = vmm_alloc();
    if (!code_page) {

      break;
    }

    // 复制代码（在释放旧页之前）
    uint8_t *src = (uint8_t *)(uintptr_t)start;
    uint8_t *dst = (uint8_t *)(uintptr_t)code_page;
    for (uint32_t i = 0; i < code_size; i++)
      dst[i] = src[i];

    // 分配新栈
    uint32_t user_stack_base = vmm_alloc();
    for (int i = 1; i < 4; i++)
      vmm_alloc();
    uint32_t user_stack = user_stack_base + 4 * 4096;

    // 现在释放旧页
    vmm_free(p->user_code);
    for (int i = 0; i < 4; i++)
      vmm_free(p->user_stack - 4096 * (i + 1));

    // patch iret frame so syscall_stub returns into the new program
    extern uint32_t syscall_kernel_esp;
    uint32_t *iret = (uint32_t *)(uintptr_t)syscall_kernel_esp;
    iret[0] = code_page;  // EIP
    iret[3] = user_stack; // ESP_user
    processes[current].ctx.esp = code_page;
    processes[current].ctx.ebp = user_stack;
    processes[current].ctx.ebx = 0;
    processes[current].ctx.esi = 0;
    processes[current].ctx.edi = 0;
    processes[current].user_code = code_page;
    processes[current].user_stack = user_stack;
    processes[current].state = PROC_RUNNING;
    processes[current].parent_pid = 0;
    break;
  }
  case SYS_WAIT:
    return process_wait();
  default:
    break;
  }
  return 0;
}
