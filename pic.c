#include "io.h"
void pic_eoi(uint8_t irq);
void pic_init(void) {
  outb(0x20, 0x11);
  io_wait(); // ICW1: 开始初始化
  outb(0xA0, 0x11);
  io_wait();

  outb(0x21, 0x20);
  io_wait(); // ICW2: PIC1 偏移 0x20
  outb(0xA1, 0x28);
  io_wait(); // ICW2: PIC2 偏移 0x28

  outb(0x21, 0x04);
  io_wait(); // ICW3: IRQ2 接从片
  outb(0xA1, 0x02);
  io_wait();

  outb(0x21, 0x01);
  io_wait(); // ICW4: 8086 模式
  outb(0xA1, 0x01);
  io_wait();

  outb(0x21, 0xFF); // 屏蔽所有 IRQ（keyboard_init 再按需开放）
  outb(0xA1, 0xFF);
}

void pic_eoi(uint8_t irq) {
  if (irq >= 8)
    outb(0xA0, 0x20); // 从片也需要 EOI
  outb(0x20, 0x20);
}
