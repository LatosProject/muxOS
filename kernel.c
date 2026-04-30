// kernel.c
#include "kernel.h"
#include "console.h"
#include "gdt.h"
#include "idt.h"
#include "io.h"
#include "keyboard.h"
#include "pic.h"
#include "pmm.h"
#include "process.h"
#include "tss.h"
#include "vga.h"
#include "vmm.h"

void panic(const char *msg);

static void pit_init(uint32_t hz) {
  uint32_t divisor = 1193180 / hz;
  outb(0x43, 0x36);
  outb(0x40, divisor & 0xFF);
  outb(0x40, (divisor >> 8) & 0xFF);
}
void task_kernel_init();

int kernel_main(uint32_t magic, multiboot_info_t *mbi) {
  if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
    PANIC("Invalid multiboot magic. Not booted by a multiboot loader.");
  }
  print("MuxOS booting...\n", VGA_COLOR_GREEN);
  gdt_init();
  tss_init();
  pic_init();
  idt_init();
  pit_init(1000);
  pmm_init(mbi);
  vmm_init();
  process_register_current();
  process_create_kernel(task_kernel_init);

  // 禁用中断，避免在创建用户进程时被调度打断
  asm volatile("cli");
  process_create_user(0); // 用户程序在 userlib.c 中
  keyboard_init();
  start_user_process(3);
  // start_user_process 不会返回，所以不需要 sti

  for (;;)
    asm volatile("hlt");
}

void panic(const char *msg) {
  clear_screen();
  print("\n=== KERNEL PANIC! ===\n", 0);
  print(msg, 0x0c);
  print("\n", 0x0c);
  for (;;)
    ;
}

void task_kernel_init() {
  print("kernel task init!\n", 0x0B);
  while (1)
    ;
}
