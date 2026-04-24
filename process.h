#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#define MAX_PROCESSES 64

typedef struct {
  uint32_t esp;
  uint32_t ebp;
  uint32_t ebx;
  uint32_t esi;
  uint32_t edi;
} context_t;

typedef struct {
  uint32_t pid;
  context_t ctx;
  uint32_t state;    // 0=ready, 1=running
  uint32_t started;  // 0=第一次运行, 1=已运行过
} process_t;

void process_schedule();
void process_create(void (*entry)());
void process_register_current();

extern process_t processes[MAX_PROCESSES];
extern int process_count;
#endif
