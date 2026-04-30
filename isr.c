#include "io.h"
#include "kernel.h"
#include "vga.h"

extern void print(const char *, unsigned char);

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

void isr13_handler(uint32_t error_code) {
  print("GPF(13)! Error code: 0x", 0x0C);
  for (int i = 28; i >= 0; i -= 4) {
    char hex = (error_code >> i) & 0xF;
    hex = hex < 10 ? '0' + hex : 'A' + hex - 10;
    char h[2] = {hex, 0};
    print(h, 0x0C);
  }
  print("\n", 0x0C);
  for (;;)
    asm volatile("hlt");
}

void isr14_handler(uint32_t error_code) {
  print("Page Fault(14)! Error code: 0x", 0x04);
  for (int i = 28; i >= 0; i -= 4) {
    char hex = (error_code >> i) & 0xF;
    hex = hex < 10 ? '0' + hex : 'A' + hex - 10;
    char h[2] = {hex, 0};
    print(h, 0x04);
  }
  print("\n", 0x04);
  uint32_t cr2;
  asm volatile("mov %%cr2, %0" : "=r"(cr2));
  print("Faulting address: 0x", 0x04);
  for (int i = 28; i >= 0; i -= 4) {
    char hex = (cr2 >> i) & 0xF;
    hex = hex < 10 ? '0' + hex : 'A' + hex - 10;
    char h[2] = {hex, 0};
    print(h, 0x04);
  }
  print("\n", 0x04);
  for (;;)
    asm volatile("hlt");
}

void isr13() {
  uint32_t error_code;
  /* prologue 已压 ebp，真正的 error code 在 [ebp+4] */
  asm volatile("mov 4(%%ebp), %0" : "=r"(error_code));
  isr13_handler(error_code);
}

void isr14() {
  uint32_t error_code;
  asm volatile("mov 4(%%ebp), %0" : "=r"(error_code));
  isr14_handler(error_code);
}
