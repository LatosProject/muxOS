// vga.h

#ifndef VGA_H
#define VGA_H

#include <stdint.h>
void print(const char *str,unsigned char color);
void vga_backspace();
void print_at(const char *str, const int x, const int y, unsigned char color);
void clear_screen();
void print_hex(uint32_t val) ;
#endif
