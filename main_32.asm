; the main kernel code in 32bit, protected mode

VIDEO_MEMORY equ 0xb8000
WHITE_ON_BLACK equ 0x0f

extern tos_kernel

.main_32:
   ; setup registers
   mov ax, DATA_SEG
   mov ds, ax
   mov es, ax
   mov fs, ax
   mov gs, ax
   mov ss, ax
   mov ebp, tos_kernel
   mov esp, ebp

   ; setup idt
   extern idt_init
   call idt_init

   ; main code

   extern terminal_clear
   call terminal_clear

   extern terminal_prompt
   call terminal_prompt

   jmp $ ; infinite loop

.maingui_32:      
   ; setup registers
   mov ax, DATA_SEG
   mov ds, ax
   mov es, ax
   mov fs, ax
   mov gs, ax
   mov ss, ax
   mov ebp, tos_kernel
   mov esp, ebp

   ; setup idt
   extern idt_init
   call idt_init

   extern memory_init
   call memory_init

   ; main code
   extern gui_init
   call gui_init

   ; setup tss
   extern tss_init
   call tss_init

   ; flush gdt
   lgdt [gdt_descriptor]
   mov ax, DATA_SEG
   mov ds, ax
   mov es, ax
   mov fs, ax
   mov gs, ax
   mov ss, ax

   ; flush tss
   mov ax, TSU_SEG | 0 ; tsu segment OR-ed with RPL 0
	ltr ax

   extern gui_draw
   call gui_draw

   ;extern terminal_prompt
   ;call terminal_prompt

   jmp $ ; infinite loop