// kernel.c
#include "kernel.h"
#include "gdt.h"
#include "idt.h"
#include "vga.h"

void panic(const char *msg);

int kernel_main() {
  gdt_init();
  idt_init();
  asm volatile("mov $0x1234, %ax\n"
               "mov %ax, %ds\n");
  return 0;
}

void panic(const char *msg) {
  clear_screen();
  print("\n=== KERNEL PANIC! ===\n", 0);
  print(msg, 0x0c);
  print("\n", 0x0c);
}