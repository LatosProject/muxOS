// kernel.c
#include "kernel.h"
#include "console.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "pic.h"
#include "pmm.h"
#include "process.h"
#include "syscall.h"
#include "vga.h"
#include "vmm.h"

void panic(const char *msg);
void task_01();
void task_02();
void task_03();
void task_04();
int kernel_main(uint32_t magic, multiboot_info_t *mbi) {
  if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
    PANIC("Invalid multiboot magic. Not booted by a multiboot loader.");
  }

  print("MuxOS booting...\n", 0x0A);
  gdt_init();
  pic_init();
  idt_init();
  pmm_init(mbi);
  vmm_init();
  process_register_current();
  process_create(task_01);
  process_create(task_02);
  process_create(task_03);
  process_create(task_04);
  keyboard_init();
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
void task_01() {
  print("task01 running...\n", 0x0B);
  sys_write(STDOUT, "hello.\n", 7);
  sys_write(STDERR, "hello.\n", 7);
  sys_exit();
  while (1)
    ;
}

void task_02() {
  print("task02 running...\n", 0x0E);
  while (1) {
    for (volatile int i = 0; i < 10000000; i++)
      ;
  }
}
void task_03() {
  print("task03 running...\n", 0x0B);
  while (1) {
    for (volatile int i = 0; i < 10000000; i++)
      ;
  }
}

void task_04() {
  print("task04 running...\n", 0x0E);
  while (1) {
    for (volatile int i = 0; i < 10000000; i++)
      ;
  }
}