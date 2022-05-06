; the main kernel code in 32bit, protected mode

VIDEO_MEMORY equ 0xb8000
WHITE_ON_BLACK equ 0x0f

.main_32:
   ; setup registers
   mov ax, DATA_SEG
   mov ds, ax
   mov es, ax
   mov fs, ax
   mov gs, ax
   mov ss, ax
   mov ebp, stack_top
   mov esp, ebp

   ; setup idt
   extern idt_init
   call idt_init

   ; main code

   extern terminal_clear
   call terminal_clear

   jmp $ ; infinite loop