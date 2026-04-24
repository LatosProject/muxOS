global irq0_stub
global context_switch
global process_enter
global process_jump
global syscall_stub
extern process_schedule
extern syscall_handler

irq0_stub:
    pusha
    mov al, 0x20
    out 0x20, al
    call process_schedule
    popa
    iret

; context_switch(old, new)
context_switch:
    mov eax, [esp+4]
    mov ecx, [esp+8]
    mov [eax+0],  esp
    mov [eax+4],  ebp
    mov [eax+8],  ebx
    mov [eax+12], esi
    mov [eax+16], edi
    mov esp, [ecx+0]
    mov ebp, [ecx+4]
    mov ebx, [ecx+8]
    mov esi, [ecx+12]
    mov edi, [ecx+16]
    ret

; process_enter(old, new)
process_enter:
    mov eax, [esp+4]
    mov ecx, [esp+8]
    mov [eax+0],  esp
    mov [eax+4],  ebp
    mov [eax+8],  ebx
    mov [eax+12], esi
    mov [eax+16], edi
    mov esp, [ecx+0]
    popa
    iret

; process_jump(new)
process_jump:
    mov ecx, [esp+4]
    mov esp, [ecx+0]
    popa
    iret

; syscall_stub — int 0x80 入口
; eax=调用号, ebx=fd, ecx=buf, edx=count
syscall_stub:
    pusha
    push edx
    push ecx
    push ebx
    push eax
    call syscall_handler
    add esp, 16
    popa
    iret
