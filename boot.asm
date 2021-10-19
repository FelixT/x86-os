[ORG 0x7c00]
   jmp start

start:
   mov ax, 0
   mov ds, ax ; data segment=0
   mov ss, ax ; stack starts at 0
   mov sp, 0x9c00 ; stack pointer starts (0x9c00-0x07c0)=2000h past code
   cld ; clear direction flag

   mov si, bootmsg
   call print

   mov si, prompt
   call print

   cli      ;no interruptions
   mov bx, 0x09   ;hardware interrupt #
   shl bx, 2   ;multiply by 4
   mov ax, 0
   mov gs, ax   ;start of memory
   mov [gs:bx], word interrupt_key
   mov [gs:bx+2], ds ; segment
   sti

hang:
   jmp hang

print_ch:
   ; print char at al
   mov ah, 0x0E ; teletype output mode for int 10h
   mov bh, 0 ; page number
   int 0x10
   ret

print:
   .ch_loop: ; loop for each char
      lodsb ; load str into al
      cmp al, 0 ; zero=end of str
      jz .done   ; get out
      call print_ch
      jmp .ch_loop
   .done:
      ret

interrupt_key:
   in al, 0x60 ; get scancode
   mov bl, al ; save to bl

   mov al, 0x20 ; End of Interrupt
   out 0x20, al ;

   movzx ebx, bl
   mov al, [keymap + ebx]
   and bl, 0x80   ; if key released
   jnz .done ; don't repeat

   call print_ch

   .done:
      iret

   bootmsg db 'Booting OS...', 13, 10, 0 ; label pointing to address of message + CR + LF
   prompt db '>', 0

keymap:
;        0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16  17  18  19
        db       0 , 27,'1','2','3','4','5','6','7','8','9','0','-','=', 8,  9, 'q','w','e','r'
        db      't','y','u','i','o','p','[',']',13 , 0 ,'a','s','d','f','g','h','j','k','l',';'
        db  "'",'`', 0 ,'\','z','x','c','v','b','n','m',',','.','/', 0 , 0 , 0 ,' ', 0 , 0
        db   0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0
        db   0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 

   times 510-($-$$) db 0 ; fill rest of 512 bytes with 0s (-2 due to signature below)
   dw 0xAA55 ; marker to show we're a bootloader to some BIOSes