;asm routines referenced in kernel

[bits 16]

section .text

%include "boot/gdt.asm"

error db 'Error!', 13, 10, 0

section .data

%include "font7.asm"

[bits 32]
%include "boot/tss.asm"

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

[bits 16]
