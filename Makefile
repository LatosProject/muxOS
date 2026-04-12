TARGET = kernel.elf
ISO = muxos.iso

all: $(ISO)

boot.o: boot.s
	nasm -f elf32 boot.s -o boot.o

kernel.o: kernel.c vga.h
	gcc -m32 -ffreestanding -fno-builtin -c kernel.c -o kernel.o

vga.o: vga.c vga.h
	gcc -m32 -ffreestanding -fno-builtin -c vga.c -o vga.o

gdt.o: gdt.c gdt.h
	gcc -m32 -ffreestanding -fno-builtin -c gdt.c -o gdt.o

idt.o: idt.c idt.h isr.c
	gcc -m32 -ffreestanding -fno-builtin -c idt.c -o idt.o

isr.o: isr.c 
	gcc -m32 -ffreestanding -fno-builtin -c isr.c -o isr.o

$(TARGET): boot.o kernel.o vga.o gdt.o idt.o isr.o linker.ld
	gcc -m32 -T linker.ld -o $(TARGET) -ffreestanding -nostdlib boot.o kernel.o vga.o gdt.o idt.o

$(ISO): $(TARGET)
	mkdir -p iso/boot/grub
	cp $(TARGET) iso/boot/
	echo 'menuentry "MuxOS" { multiboot /boot/$(TARGET) }' > iso/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) iso

run: $(ISO)
	qemu-system-i386 -cdrom $(ISO)

clean:
	rm -f *.o $(TARGET) $(ISO)
	rm -rf iso
