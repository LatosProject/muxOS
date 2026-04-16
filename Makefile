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

pic.o: pic.c pic.h io.h
	gcc -m32 -ffreestanding -fno-builtin -c pic.c -o pic.o

keyboard.o: keyboard.c keyboard.h pic.h io.h vga.h
	gcc -m32 -ffreestanding -fno-builtin -mgeneral-regs-only -c keyboard.c -o keyboard.o

console.o: console.c console.h vga.h
	gcc -m32 -ffreestanding -fno-builtin -c console.c -o console.o

pmm.o: pmm.c pmm.h
	gcc -m32 -ffreestanding -fno-builtin -c pmm.c -o pmm.o

vmm.o: vmm.c vmm.h pmm.h
	gcc -m32 -ffreestanding -fno-builtin -c vmm.c -o vmm.o

serial.o: serial.c serial.h io.h
	gcc -m32 -ffreestanding -fno-builtin -c serial.c -o serial.o


$(TARGET): boot.o kernel.o vga.o gdt.o idt.o isr.o pic.o keyboard.o console.o pmm.o vmm.o serial.o linker.ld
	gcc -m32 -T linker.ld -o $(TARGET) -ffreestanding -nostdlib boot.o kernel.o vga.o gdt.o idt.o isr.o pic.o keyboard.o console.o pmm.o vmm.o serial.o

$(ISO): $(TARGET)
	mkdir -p iso/boot/grub
	cp $(TARGET) iso/boot/
	echo 'menuentry "MuxOS" { multiboot /boot/$(TARGET) }' > iso/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) iso

run: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -display gtk
clean:
	rm -f *.o $(TARGET) $(ISO)
	rm -rf iso
