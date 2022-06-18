[bits 32]

loop:
jmp loop

times 512-($-$$) db 0