#pragma once
#include <stdint.h>

#define KB_BUFFER_SIZE 128



struct interrupt_frame {
    uint32_t ip, cs, flags;
};

__attribute__((interrupt))
void keyboard_handler(struct interrupt_frame *frame);

void keyboard_init(void);
char kb_getchar();