; real mode interrupt handler

setup_interrupts:
   cli ; no interruptions

   mov ax, 0
   mov gs, ax

   %ifdef COMMENT
   mov bx, 0 ; ivt offset
   .loop:
      cmp bx, 0x20 ; skip PIT
      je .next

      cmp bx, 0x0040
      je .done

      mov [gs:bx], word interrupt_default
      mov [gs:bx+2], word ds ; segment

      jmp .next

      .next:
         add bx, 4
         jmp .loop

      .done:
   %endif
      
   ; setup int 9 keyboard handler
   mov bx, 0x09 ; hardware interrupt #
   shl bx, 2 ; multiply by 4
   mov [gs:bx], word interrupt_key
   mov [gs:bx+2], word ds ; segment
   sti ; reenable interrupts
   ret

interrupt_default:
   push ax
   push si
   mov al, 0x20 ; End of Interrupt
   out 0x20, al ;

   mov si, int_msg
   call print
   pop si
   pop ax
   iret

interrupt_key:
   cli
   in al, 0x60 ; get scancode
   mov bl, al ; save to bl
   mov al, 0x20 ; End of Interrupt
   out 0x20, al
   
   movzx ebx, bl
   test bl, 0x80 ; check if bit 7 is set (key release)
   jz .done
   
   ; handle key release
   and bl, 0x7F ; clear bit 7 to get the actual scancode
   mov cl, [keymap + ebx] ; convert to ascii
   mov al, cl ; ascii key
   call terminal_keypress
   jmp .done
   
.done:
   sti
   iret

keymap:
   db       0 , 27,'1','2','3','4','5','6','7','8','9','0','-','=', 8,  9, 'q','w','e','r'
   db      't','y','u','i','o','p','[',']',13 , 0 ,'a','s','d','f','g','h','j','k','l',';'
   db  "'",'`', 0 ,'\','z','x','c','v','b','n','m',',','.','/', 0 , 0 , 0 ,' ', 0 , 0
   db   0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0
   db   0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 

int_msg db 'Interrupt!', 0