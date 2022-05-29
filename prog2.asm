[bits 32]

loop:
mov eax, 2
mov ebx, 69
int 0x30
jmp loop
ret

times 512-($-$$) db 0