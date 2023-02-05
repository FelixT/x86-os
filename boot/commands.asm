
[bits 32]
%include "boot/main_32.asm"
[bits 16]

cmd_cli:
   ; enter protected mode
   cli ; disable interrupts
   lgdt [gdt_descriptor] ; load GDT register with start address of Global Descriptor Table
   mov eax, cr0
   or eax, 0x1
   mov cr0, eax
   
   jmp CODE_SEG:main_32

cmd_gui:
   call vesa_init ; has to be done in real mode
   
   ; enter protected mode
   cli ; disable interrupts
   lgdt [gdt_descriptor] ; load GDT register with start address of Global Descriptor Table
   mov eax, cr0
   or eax, 0x1
   mov cr0, eax
   
   jmp CODE_SEG:maingui_32   

cmd_cli_str db 'cli', 0
cmd_gui_str db 'gui', 0

check_cmd:
   ; 'protected'
   mov di, cmd_cli_str
   call compare_strings
   cmp ax, 1
   jz cmd_cli

   ; 'gui'
   mov di, cmd_gui_str
   call compare_strings
   cmp ax, 1
   jz cmd_gui

   ret
