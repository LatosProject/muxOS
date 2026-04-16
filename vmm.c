// vmm.c
#include "vmm.h"
#include "pmm.h"
#include "vga.h"

static uint32_t page_directory[1024] __attribute__((aligned(4096)));

void vmm_init() {
  uint32_t *page_table = (uint32_t *)pmm_alloc();

  for (int i = 0; i < 1024; i++)
    page_table[i] = (i * 0x1000) | PAGE_PRESENT | PAGE_WRITE;

  page_directory[0] = (uint32_t)page_table | PAGE_PRESENT | PAGE_WRITE;
  for (int i = 1; i < 1024; i++)
    page_directory[i] = 0;

  // 写入页目录地址到 CR3
  asm volatile("mov %0, %%cr3" ::"r"((uint32_t)page_directory));

  // 设置 CR0.PG 位
  uint32_t cr0;
  asm volatile("mov %%cr0, %0" : "=r"(cr0));
  cr0 |= 0x80000000;
  asm volatile("mov %0, %%cr0" ::"r"(cr0));
  print("[OK] VMM init\n", 0);
}