// vga.c

#include "vga.h"
#include "io.h" // 需要 outb
static void update_hw_cursor();
static int cursor_x = 0;
static int cursor_y = 0;
static char *vga = (char *)0xb8000;
static void scroll();
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
  if (cursor_y >= 25) {
    scroll();
  }
  update_hw_cursor();
}
static void update_hw_cursor() {
  uint16_t pos = cursor_y * 80 + cursor_x;
  outb(0x3D4, 0x0F);
  outb(0x3D5, pos & 0xFF);
  outb(0x3D4, 0x0E);
  outb(0x3D5, (pos >> 8) & 0xFF);
}

static void scroll() {
  for (int y = 0; y < 24; y++) {
    for (int x = 0; x < 80; x++) {
      int dst = (y * 80 + x) * 2;
      int src = ((y + 1) * 80 + x) * 2;
      vga[dst] = vga[src];
    }
  }
  for (int x = 0; x < 80; x++) {
    int offset = (24 * 80 + x) * 2;
    vga[offset] = ' ';
    vga[offset + 1] = 0x07;
  }
  cursor_y = 24;
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