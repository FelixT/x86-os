setup_interrupts:
   cli ; no interruptions
   mov bx, 0x09 ; hardware interrupt #
   shl bx, 2 ; multiply by 4
   mov ax, 0
   mov gs, ax ; start of memory
   mov [gs:bx], word interrupt_key
   mov [gs:bx+2], ds ; segment
   sti ; reenable interrupts
   ret

interrupt_key:
   in al, 0x60 ; get scancode
   mov bl, al ; save to bl

   mov al, 0x20 ; End of Interrupt
   out 0x20, al ;

   movzx ebx, bl
   mov al, [keymap + ebx] ; convert to ascii

   and bl, 0x80 ; if key released
   jnz .done ; don't repeat

   call print_ch

   cmp al, 0x0d ; if key is return
   jz .return

   jmp .done

   .return:
      mov al, 0x0a ; print newline char
      call print_ch
      jmp read_kernel

   .done:
      iret