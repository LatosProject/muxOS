TARGET = kernel.elf
ISO = muxos.iso

all: $(ISO)

boot.o: boot.s
	nasm -f elf32 boot.s -o boot.o

switch.o: switch.s
	nasm -f elf32 switch.s -o switch.o

user_entry.o: user_entry.s
	nasm -f elf32 user_entry.s -o user_entry.o

user_prog.o: user_prog.s
	nasm -f elf32 user_prog.s -o user_prog.o

user_task.o: user_task.c
	gcc -m32 -ffreestanding -fno-builtin -fno-pic -c user_task.c -o user_task.o

user_start.o: user_start.s
	nasm -f elf32 user_start.s -o user_start.o

user_simple.o: user_simple.s
	nasm -f elf32 user_simple.s -o user_simple.o

user_prog_c.o: user_prog_c.s
	nasm -f elf32 user_prog_c.s -o user_prog_c.o

userlib.o: userlib.c
	gcc -m32 -ffreestanding -fno-builtin -fno-pic -O0 -c userlib.c -o userlib.o

user_crt.o: user_crt.s
	nasm -f elf32 user_crt.s -o user_crt.o

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

process.o: process.c process.h 
	gcc -m32 -ffreestanding -fno-builtin -c process.c -o process.o

syscall.o: syscall.c syscall.h
	gcc -m32 -ffreestanding -fno-builtin -c syscall.c -o syscall.o

tss.o: tss.c tss.h gdt.h
	gcc -m32 -ffreestanding -fno-builtin -c tss.c -o tss.o

$(TARGET): boot.o kernel.o vga.o gdt.o idt.o isr.o pic.o keyboard.o console.o pmm.o vmm.o serial.o process.o switch.o syscall.o tss.o user_entry.o user_crt.o userlib.o linker.ld
	gcc -m32 -T linker.ld -o $(TARGET) -ffreestanding -nostdlib boot.o kernel.o vga.o gdt.o idt.o isr.o pic.o keyboard.o console.o pmm.o vmm.o serial.o process.o switch.o syscall.o tss.o user_entry.o user_crt.o userlib.o

$(ISO): $(TARGET)
	mkdir -p iso/boot/grub
	cp $(TARGET) iso/boot/
	printf 'set timeout=1\nset default=0\nmenuentry "MuxOS" { multiboot /boot/$(TARGET) }\n' > iso/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) iso

run: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -serial stdio -display gtk
clean:
	rm -f *.o $(TARGET) $(ISO)
	rm -rf iso
