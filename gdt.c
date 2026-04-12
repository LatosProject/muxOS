#include "gdt.h"
#include "vga.h"
static struct gdt_entry gdt[3];
static struct gdt_ptr gp;

static void gdt_set(int i, unsigned int base, unsigned int limit,
                    unsigned int access, unsigned int gran) {
  gdt[i].base_low = base & 0xFFFF;
  gdt[i].base_middle = (base >> 16) & 0xFF;
  gdt[i].base_high = (base >> 24) & 0xFF;

  gdt[i].limit_low = limit & 0xFFFF;
  gdt[i].granularity = (limit >> 16) & 0x0F;

  gdt[i].granularity |= gran & 0xF0;

  gdt[i].access = access;
}

void gdt_init() {
  gp.limit = sizeof(gdt) - 1;
  gp.base = (unsigned int)(unsigned long)&gdt;

  gdt_set(0, 0, 0, 0, 0);                // null descriptor
  gdt_set(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // kernel code  (selector 0x08)
  gdt_set(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // kernel data  (selector 0x10)

  asm volatile("lgdt %0" : : "m"(gp));

  // reload all data segment registers to use selector 0x10
  asm volatile("mov $0x10, %%ax\n"
               "mov %%ax, %%ds\n"
               "mov %%ax, %%es\n"
               "mov %%ax, %%fs\n"
               "mov %%ax, %%gs\n"
               "mov %%ax, %%ss\n"
               "ljmp $0x08, $1f\n" // far jump to reload CS with selector 0x08
               "1:\n"
               :
               :
               : "eax");
  print("[OK] GDT init\n", 0);
}
