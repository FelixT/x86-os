gdt_start:

gdt_null:
    dd 0x0
    dd 0x0

gdt_code:
    dw 0xffff ; Limit
    dw 0x0     ; Base
    db 0x0	 ; Base 23:16
    db 10011010b
    db 11001111b
    db 0x0

gdt_data:
    dw 0xffff    ; Limit
    dw 0x0    ; Base
    db 0x0	 ; Base 23:16
    db 10010010b
    db 11001111b
    db 0x0

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start  ; GDT size
    dd gdt_start

; constants to get address of gdt
CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start