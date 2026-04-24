#include "process.h"
#include "pmm.h"
#include <stdint.h>

process_t processes[MAX_PROCESSES];
int current = 0;
int process_count = 0;

void context_switch(context_t *old, context_t *new);
void process_enter(context_t *old, context_t *new);

void process_register_current() {
  processes[0].pid = 0;
  processes[0].state = 1;
  processes[0].started = 1;
  process_count = 1;
}

void process_schedule() {
  int next = (current + 1) % process_count;
  if (next == current)
    return;
  int old = current;
  current = next;
  if (!processes[next].started) {
    processes[next].started = 1;
    process_enter(&processes[old].ctx, &processes[current].ctx);
  } else {
    context_switch(&processes[old].ctx, &processes[current].ctx);
  }
}

void process_create(void (*entry)()) {
  uint32_t stack_top = pmm_alloc() + 4096;

  // 伪造 iret 帧: eflags, cs, eip
  stack_top -= 4;
  *(uint32_t *)stack_top = 0x200; // eflags (IF=1)
  stack_top -= 4;
  *(uint32_t *)stack_top = 0x08; // cs
  stack_top -= 4;
  *(uint32_t *)stack_top = (uint32_t)entry; // eip

  // 伪造 pusha 帧: eax ecx edx ebx esp ebp esi edi
  stack_top -= 4;
  *(uint32_t *)stack_top = 0; // eax
  stack_top -= 4;
  *(uint32_t *)stack_top = 0; // ecx
  stack_top -= 4;
  *(uint32_t *)stack_top = 0; // edx
  stack_top -= 4;
  *(uint32_t *)stack_top = 0; // ebx
  stack_top -= 4;
  *(uint32_t *)stack_top = 0; // esp
  stack_top -= 4;
  *(uint32_t *)stack_top = 0; // ebp
  stack_top -= 4;
  *(uint32_t *)stack_top = 0; // esi
  stack_top -= 4;
  *(uint32_t *)stack_top = 0; // edi

  processes[process_count].pid = process_count + 1;
  processes[process_count].ctx.esp = stack_top;
  processes[process_count].ctx.ebp = 0;
  processes[process_count].ctx.ebx = 0;
  processes[process_count].ctx.esi = 0;
  processes[process_count].ctx.edi = 0;
  processes[process_count].started = 0;
  process_count++;
}

void process_jump(context_t *new);

void process_exit() {
  for (int i = current; i < process_count - 1; i++)
    processes[i] = processes[i + 1];
  process_count--;

  if (process_count == 0) {
    for (;;)
      asm volatile("hlt");
  }

  if (current >= process_count)
    current = 0;

  // 跳过 process 0（kernel_main）
  if (current == 0 && process_count > 1)
    current = 1;

  processes[current].started = 1;
  process_jump(&processes[current].ctx);
}
