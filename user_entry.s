global enter_usermode

enter_usermode:
    mov esi, [esp+4]
    mov edi, [esp+8]

    push 0x23
    push edi
    push 0x202
    push 0x1B
    push esi
    iret
