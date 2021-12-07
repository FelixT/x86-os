[ORG 0x7e00]
   jmp kmain
   %include "terminal.asm"
   %include "terminal_plus.asm"
   %include "interrupts.asm"
   %include "gui.asm"

kmain:
   mov si, boot1msg
   call print

   call setup_interrupts

   call terminal_newline

   jmp hang


videoinfo:
   ; get videoinfo as al
   mov ah, 0x0f
   int 0x10
   mov ah, 0

   ; print video info
   call print_num

   ; print CR LF
   mov al, 13
   call print_ch
   mov al, 10
   call print_ch

   ret

check_cmd:
   ; 'gui'
   mov di, cmd_gui
   call compare_strings
   cmp ax, 1
   jz load_gui

   ; 'videoinfo'
   mov di, cmd_videoinfo
   call compare_strings
   cmp ax, 1
   jz videoinfo

   ret

load_gui:
   call gui_enable

   call drawrect

   ret

hang:
   jmp hang

   ; strings
   boot1msg db 'Loaded into bootloader1', 13, 10, 0 ; label pointing to address of message + CR + LF
   cmd_gui db 'gui', 0
   cmd_videoinfo db 'videoinfo', 0

   times 8192-($-$$) db 0 ; fill rest of 8192 bytes with 0s