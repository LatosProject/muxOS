#include "pmm.h"
#include <stdint.h>
#define PAGE_PRESENT 0x1 // 页存在
#define PAGE_WRITE 0x2   // 可写
#define PAGE_USER 0x4    // 用户态可访问
void vmm_init();
uint32_t vmm_alloc();
void vmm_free(uint32_t virt);