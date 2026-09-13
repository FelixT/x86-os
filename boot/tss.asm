; extern tos_kernel (0x164000: see memory.h)

global tss_start
tss_start:
   dd 0
   dd 0x164000 ; esp0, kernel stack pointer
   dd DATA_SEG ; ss0, kernel stack segment
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0 ; es
   dd 0 ; cs
   dd 0 ; ss
   dd 0 ; ds
   dd 0 ; fs
   dd 0 ; gs
   dd 0
   dw 0
   dw 0

global tss_end
tss_end:

global df_tss_start
df_tss_start:
   dd 0
   dd 0x168000 ; esp0, kernel stack pointer
   dd DATA_SEG ; ss0, kernel stack segment
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0x2 ; eflags - interrupts disabled
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0x168000 ; esp
   dd 0
   dd 0
   dd 0
   dd DATA_SEG ; es
   dd CODE_SEG ; cs - kernel code segment
   dd DATA_SEG ; ss
   dd DATA_SEG ; ds
   dd DATA_SEG ; fs
   dd DATA_SEG ; gs
   dd 0
   dw 0
   dw 0

global df_tss_end
df_tss_end:

global load_page_dir
load_page_dir:
   ; load page directory
   push ebp
   mov ebp, esp
   mov eax, [esp+8]
   mov cr3, eax
   mov esp, ebp
   pop ebp
   ret

global page_enable
page_enable:
   push ebp
   mov ebp, esp
   mov eax, cr0
   or eax, 0x80000000 ;0x80000001
   mov cr0, eax
   mov esp, ebp
   pop ebp
   ret