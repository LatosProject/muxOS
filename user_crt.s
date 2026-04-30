bits 32

extern user_main

global user_c_start

user_c_start:
    ; iret 已经把用户栈顶恢复到 ESP，直接在当前栈上分配局部空间
    sub esp, 1024

    ; 计算 user_main 的实际地址
    call .get_ip
.get_ip:
    pop ebx
    sub ebx, (.get_ip - user_c_start)

    mov eax, user_main
    sub eax, user_c_start
    add eax, ebx

    call eax
.hang:
    jmp .hang
