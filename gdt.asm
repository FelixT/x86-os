gdt_start:

gdt_null:
    dd 0x0
    dd 0x0

gdt_code: ; base = 0, limit = 0xFFFFF, spanning full 4GiB address space
    dw 0xffff ; limit low (bits 0-15)
    dw 0x0     ; base low (bits 0-15)
    db 0x0	 ; base mid (bits 16-23)
    db 0x9a ; access byte, 0x9a=kernel code
    db 0xcf ; flags/granularity, 0xc=32 bit protected & limit is in 4KiB blocks
            ; 0xf=limit high (bits 16-19) 
    db 0x0 ; base high (bits 24-31)

gdt_data:
    dw 0xffff
    dw 0x0
    db 0x0
    db 0x92 ; 0x92=kernel data
    db 0xcf
    db 0x0

gdt_usercode:
    dw 0xffff
    dw 0x0
    db 0x0
    db 0xfa ; 0xFA=user code
    db 0xcf
    db 0x0

gdt_userdata:
    dw 0xffff
    dw 0x0
    db 0x0
    db 0xf2 ; 0xF2=user data
    db 0xcf
    db 0x0

global gdt_tss
gdt_tss:
    dw 0x0 ; base & limit blank as these are determined by tss size
    dw 0x0
    db 0x0
    db 0x89
    db 0x0
    db 0x0

gdt_end:

gdt_descriptor:
    dw gdt_end - (gdt_start+1)  ; GDT size
    dd gdt_start

; constants to get address of gdt
CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

USER_CODE_SEG equ gdt_usercode - gdt_start
USER_DATA_SEG equ gdt_userdata - gdt_start
TSU_SEG equ gdt_tss - gdt_start