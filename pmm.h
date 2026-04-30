#ifndef PMM_H
#define PMM_H
#include "kernel.h"
#include <stdint.h>
#define PAGE_SIZE 4096
#define MAX_PAGES 1048576  /* 1M 页 × 4KB = 4GB */
void pmm_init(multiboot_info_t *mbi);
uint32_t pmm_alloc();
void pmm_free(uint32_t addr);
void pmm_mark_used(uint32_t start, uint32_t length);
void pmm_mark_free(uint32_t start, uint32_t length);
#endif