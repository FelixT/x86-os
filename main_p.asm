[bits 16]

%include "gdt.asm"

; allocate new 16 byte aligned, 8 KiB stack
section .bss

align 16
stack_bottom:
resb 8192
stack_top:

section .text

jmp kmain

;%include "interrupts.asm"

global kmain
kmain:
   mov esp, stack_top

   jmp $ ; infinite loop