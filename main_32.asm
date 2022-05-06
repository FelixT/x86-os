; the main kernel code in 32bit, protected mode

VIDEO_MEMORY equ 0xb8000
WHITE_ON_BLACK equ 0x0f

.main_32:
   mov ax, DATA_SEG
   mov ds, ax
   mov es, ax
   mov fs, ax
   mov gs, ax
   mov ss, ax
   mov ebp, stack_top
   mov esp, ebp

   ; main code

   extern terminal_clear
   call terminal_clear

   mov ebx, cmd_protected
   call .print_32


   jmp $ ; infinite loop

; in=AX
.print_32:
   pusha
   mov edx, VIDEO_MEMORY
   .print_loop:
      mov al, [ebx]
      mov ah, WHITE_ON_BLACK
      cmp al, 0
      je .print_done
      mov [edx], ax
      add ebx, 1
      add edx, 2
      jmp .print_loop
   .print_done:
      popa

