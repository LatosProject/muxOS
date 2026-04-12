// vga.h

#ifndef VGA_H
#define VGA_H

void print(const char *str,unsigned char color);
void print_at(const char *str, const int x, const int y, unsigned char color);
void clear_screen();
#endif
