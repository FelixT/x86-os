; bootloader 1
; allows us to change video mode from real mode then enter protected mode
; include command line interface
; calls cboot which then loads kernel and jumps to it

[bits 16]

section .text

jmp kmain

%include "boot/terminal.asm"
%include "boot/interrupts.asm"
%include "boot/vesa.asm"
%include "boot/gdt.asm"

; put data in text section to force it to be located within 0xFFFF and hence usable in real mode
boot1msg db 'Loaded into f3sys bootloader', 13, 10, 13, 10, '>> Type "gui"/[return] or "cli" to continue', 13, 10, 0
error db 'Error!', 13, 10, 0

global kmain
kmain:
   ; entry
   mov sp, 0x7a00 ; set stack to before bootloader for now

   ; clear screen
   mov ah, 0x06 ; scroll func
   mov al, 0 ; clear
   mov bh, 0x0f ; white on black
   mov ch, 0
   mov cl, 0
   mov dh, 24
   mov dl, 79
   int 0x10
   ; reset cursor
   mov ah, 0x02 ; set cursor pos
   mov bh, 0 ; page
   mov dh, 0 ; row
   mov dl, 0 ; col
   int 0x10

   mov si, boot1msg
   call print

   call setup_interrupts
   call terminal_newline

   jmp $

; may be located after 0xFFFF hence usable only in protected mode
section .data

%include "font7.asm"

[bits 32]
%include "boot/tss.asm"
[bits 16]
