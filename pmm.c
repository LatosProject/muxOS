// pmm.c
#include "pmm.h"
#include "vga.h"
#include <stdint.h>
static uint8_t bitmap[MAX_PAGES / 8];
void pmm_mark_free(uint32_t start, uint32_t length);
void pmm_mark_used(uint32_t start, uint32_t length);

extern uint32_t _kernel_end;

void pmm_init(multiboot_info_t *mbi) {
  // 获取物理内存可用地址
  for (int i = 0; i < MAX_PAGES / 8; i++) {
    bitmap[i] = 0xFF;
  }
  if (CHECK_FLAG(mbi->flags, 6)) {
    multiboot_memory_map_t *mmap;
    for (mmap = (multiboot_memory_map_t *)mbi->mmap_addr;
         (unsigned long)mmap < mbi->mmap_addr + mbi->mmap_length;
         mmap = (multiboot_memory_map_t *)((unsigned long)mmap + mmap->size +
                                           sizeof(mmap->size))) {
      if (mmap->type == 1 && (uint32_t)mmap->addr >= 0x100000) {
        uint32_t start = (uint32_t)mmap->addr;
        uint32_t length = (uint32_t)mmap->len;
        pmm_mark_free(start, length);
      }
    }
  }
  // 把内核自身占用的区域标记为占用
  pmm_mark_used(0x100000, (uint32_t)(uintptr_t)&_kernel_end - 0x100000);
  print("[OK] PMM init\n", 0);
}

uint32_t pmm_alloc() {
  for (int i = 0; i < MAX_PAGES; i++) {
    if (!(bitmap[i / 8] & (1 << (i % 8)))) {
      bitmap[i / 8] |= (1 << (i % 8));
      return i * PAGE_SIZE;
    }
  }
  PANIC("No free memory pages available");
}

// 标记一段内存为空闲
void pmm_mark_free(uint32_t start, uint32_t length) {
  uint32_t page = start / PAGE_SIZE;
  uint32_t count = length / PAGE_SIZE;
  for (uint32_t i = page; i < page + count; i++)
    bitmap[i / 8] &= ~(1 << (i % 8)); // 清bit
}

void pmm_mark_used(uint32_t start, uint32_t length) {
  uint32_t page = start / PAGE_SIZE;
  uint32_t count = (length + PAGE_SIZE - 1) / PAGE_SIZE;
  for (uint32_t i = page; i < page + count; i++)
    bitmap[i / 8] |= (1 << (i % 8)); // 置bit
}