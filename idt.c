#include "idt.h"
#include "vga.h"
#include "keyboard.h"
extern void isr0();
extern void isr6();
extern void isr13();
static idt_entry_t idt[IDT_SIZE];
static idt_ptr_t idtp;

static void idt_set(int i, uint32_t handler, uint16_t selector, uint8_t flags) {
  idt[i].offset_low = handler & 0xFFFF;
  idt[i].offset_high = (handler >> 16) & 0xFFFF;
  idt[i].selector = selector;
  idt[i].zero = 0;
  idt[i].type_attr = flags;
}

void idt_init() {
  idtp.limit = sizeof(idt) - 1;
  idtp.base = (uint32_t)(unsigned long)&idt;

  for (int i = 0; i < IDT_SIZE; i++)
    idt_set(i, 0, 0, 0);

  idt_set(0, (uint32_t)(unsigned long)isr0, 0x08, 0x8E);
  idt_set(6, (uint32_t)(unsigned long)isr6, 0x08, 0x8E);
  idt_set(13, (uint32_t)(unsigned long)isr13, 0x08, 0x8E);
  idt_set(0x21, (uint32_t)(unsigned long)keyboard_handler, 0x08, 0x8E);
  asm volatile("lidt %0" : : "m"(idtp));
  asm volatile("sti");

  print("[OK] IDT init\n", 0);
}