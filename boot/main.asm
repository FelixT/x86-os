[bits 16]

section .text

jmp kmain

%include "boot/terminal.asm"
%include "boot/interrupts.asm"
%include "boot/vesa.asm"
%include "boot/gdt.asm"

; put data in text section to force it to be located within 0xFFFF and hence usable in real mode
boot1msg db 'Loaded into bootloader1', 13, 10, 0 ; label pointing to address of message + CR + LF
error db 'Error!', 13, 10, 0

global kmain
kmain:
   mov sp, 0x7a00 ; set stack to before bootloader for now

   ;extern terminal_clear
   ;call terminal_clear

   mov si, boot1msg
   call print

   call setup_interrupts
   call terminal_newline

   jmp $

; may located after 0xFFFF hence usable only in protected mode
section .data

%include "font7.asm"

[bits 32]
%include "boot/tss.asm"
[bits 16]
