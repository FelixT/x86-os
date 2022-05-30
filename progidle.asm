[bits 32]

loop:
hlt
jmp loop

times 512-($-$$) db 0