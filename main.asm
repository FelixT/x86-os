[ORG 0x7e00]
   jmp kmain
   %include "terminal.asm"
   %include "interrupts.asm"


kmain:
   mov si, boot1msg
   call print

   call setup_interrupts

   call terminal_newline

   jmp hang

graphics:
   mov ah, 0x00 ; set video mode
   mov al, 0x13 ; video mode = standard vga
   int 0x10
   ret

setpixel:
   mov ebx, 0xA0000
   mov ah, 4 ; colour
   mov bh, 200 ; offset
   mov ax, 0xA000
   mov gs, ax
   mov [gs:200], ah
   ret

check_cmd:
   mov di, cmd_gui
   call compare_strings
   cmp ax, 1
   jz load_gui
   ret

load_gui:
   call graphics

   call setpixel

hang:
   jmp hang


   ; strings
   boot1msg db 'Loaded into bootloader1', 13, 10, 0 ; label pointing to address of message + CR + LF
   cmd_gui db 'gui', 0

   times 8192-($-$$) db 0 ; fill rest of 8192 bytes with 0s