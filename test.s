section .data
msg db '0',0x0A
section  .text
   global _start     ;must be declared for linker (ld)
   
_start:          

    mov eax ,4
    mov ebx,1
    mov ecx,msg
    mov edx,2
    int 0x80

    mov eax ,1
    xor ebx ,ebx
    int 0x80