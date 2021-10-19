; boot.asm
   mov ax, 0x07c0
   mov ds, ax ; set data segment to 0x07c0, where the bios loads the bootloader

   cld ; clear direction flag
   mov si, msg
   call print

hang:
   jmp hang

msg db 'Booting OS...', 13, 10, 0 ; message + CR + LF

print:
   .ch_loop: ; loop for each char
      lodsb ; load str into al
      cmp al, 0 ; zero=end of str
      jz .done   ; get out
      mov ah, 0x0E ; teletype output mode for int 10h
      mov bh, 0 ; page number
      int 0x10
      jmp .ch_loop
   .done:
      ret

   times 510-($-$$) db 0 ; fill rest of space with 0s
   db 0x55 ; markers to show we're a bootloader
   db 0xAA