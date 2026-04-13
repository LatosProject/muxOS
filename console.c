#include "console.h"
#include "keyboard.h"
void readline(char *buf, int max_len) {
  int i = 0;
  while (i < max_len) {
    buf[i] = kb_getchar();
    if (buf[i] == '\n') {
      break;
    }
    if (buf[i] == '\b') {
      if (i > 0) {
        buf[i] = '\0';
        i--;
      }
    } else {
      i++;
    }
  }
  buf[i] = '\0';
}