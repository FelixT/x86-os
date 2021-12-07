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
   mov cl, [keymap + ebx] ; convert to ascii

   and bl, 0x80 ; if key released
   jnz .done ; don't repeat

   ; if cli
   mov ah, 0x0f
   int 0x10
   mov ah, 0
   cmp ax, 0x03 ; mode = 3
   jz .cli
   jmp .gui ; else if gui

   .cli:
      mov al, cl ; ascii key
      call terminal_keypress
      jmp .done

   .gui:
      mov ah, 0x00 ; set video mode
      mov al, 0x03 ; video mode = 80x25 16 color text vga
      int 0x10
      jmp .done

   .done:
      iret

keymap:
   db       0 , 27,'1','2','3','4','5','6','7','8','9','0','-','=', 8,  9, 'q','w','e','r'
   db      't','y','u','i','o','p','[',']',13 , 0 ,'a','s','d','f','g','h','j','k','l',';'
   db  "'",'`', 0 ,'\','z','x','c','v','b','n','m',',','.','/', 0 , 0 , 0 ,' ', 0 , 0
   db   0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0
   db   0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 