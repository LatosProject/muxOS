#include "kernel.h"

void isr0() {
  PANIC("INFO: Divide by zero");
  for (;;)
    ;
}
void isr6() {
  PANIC("INFO: Invalid Opcode");
  for (;;) {
    asm volatile("hlt");
  }
}

void isr13() {
  PANIC("INFO: General Protection Fault in kernel");
  for (;;)
    asm volatile("hlt");
}