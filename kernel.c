// kernel.c
#include "kernel.h"
#include "console.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "pic.h"
#include "vga.h"

void panic(const char *msg);

int kernel_main() {
  clear_screen();
  print("MuxOS booting...\n", 0x0A);
  gdt_init();
  pic_init();
  idt_init();
  keyboard_init();
  while (1) {
    char buf[128] = {0};
    print("muxOS> ", 0x07);
    readline(buf, 128);
    print("\n", 0);
  }
  for (;;)
    asm volatile("hlt");
}

void panic(const char *msg) {
  clear_screen();
  print("\n=== KERNEL PANIC! ===\n", 0);
  print(msg, 0x0c);
  print("\n", 0x0c);
}