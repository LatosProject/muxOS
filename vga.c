// vga.c

#include "vga.h"

static int cursor_x = 0;
static int cursor_y = 0;
static char *vga = (char *)0xb8000;

void print(const char *str, unsigned char color) {
  if (color == 0)
    color = 0x07;
  for (int i = 0; str[i]; i++) {
    if (str[i] == '\n') {
      cursor_y++;
      cursor_x = 0;
      continue;
    }
    int offset = (cursor_y * 80 + cursor_x) * 2;
    vga[offset] = str[i];
    vga[offset + 1] = color ? color : 0x07;
    cursor_x++;
  }
  if (cursor_x >= 80) {
    cursor_y++;
    cursor_x = 0;
  }
}

void print_at(const char *str, const int x, const int y, unsigned char color) {
  cursor_x = x;
  cursor_y = y;
  if (color == 0)
    color = 0x07;
  for (int i = 0; str[i]; i++) {
    int offset = (y * 80 + x + i) * 2;
    vga[offset] = str[i];
    vga[offset + 1] = color ? color : 0x07;
  }
}

void clear_screen() {
  for (int y = 0; y < 25; y++) {
    for (int x = 0; x < 80; x++) {
      int offset = (y * 80 + x) * 2;
      vga[offset] = ' ';
      vga[offset + 1] = 0x07;
    }
  }
  cursor_x = 0;
  cursor_y = 0;
}