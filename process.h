#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#define MAX_PROCESSES 64
#define PROC_RUNNING  1
#define PROC_ZOMBIE 2 // 已退出但父进程还没 wait

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
  uint32_t state;
  uint32_t started;
  uint32_t kernel_stack;
  uint32_t sleep_ticks;
  uint32_t user_code;   // code_page 虚拟地址（用于 exec 释放）
  uint32_t user_stack;  // 用户栈顶虚拟地址（用于 exec 释放）
  uint32_t parent_pid;   // 父进程 pid
  uint32_t exit_code;    // 退出时存在这里
} process_t;

void process_schedule();
int  process_tick();
void process_create_kernel(void (*entry)());
void process_create_user(void (*entry)());
void process_register_current();
void start_user_process(int pid);
void process_sleep(uint32_t ticks);
void process_exit();
int  process_fork(uint32_t child_eax_ret);
int  process_wait();

extern process_t processes[MAX_PROCESSES];
extern int process_count;
extern int current;
extern uint32_t syscall_kernel_esp;
#endif
