; the main kernel code in 32bit, protected mode

VIDEO_MEMORY equ 0xb8000
WHITE_ON_BLACK equ 0x0f

; extern tos_kernel (0x06422000: see memory.h)

main_32:
   ; setup registers
   mov ax, DATA_SEG
   mov ds, ax
   mov es, ax
   mov fs, ax
   mov gs, ax
   mov ss, ax
   mov ebp, 0x06422000
   mov esp, ebp

   extern cboot
   call cboot

   jmp $ ; infinite loop

global gdt_flush
gdt_flush:
   ; flush gdt
   lgdt [gdt_descriptor]
   mov ax, DATA_SEG
   mov ds, ax
   mov es, ax
   mov fs, ax
   mov gs, ax
   mov ss, ax
   ret

global tss_flush
tss_flush:
   mov ax, TSU_SEG | 0 ; tsu segment OR-ed with RPL 0
	ltr ax
   ret

global videomode
videomode db 0
; 0 - cli
; 1 - gui