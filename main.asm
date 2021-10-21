[ORG 0x7e00]

kmain:
    mov si, boot1msg
    call print
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
      jz .done ; get out
      call print_ch
      jmp .ch_loop
   .done:
      ret

hang:
   jmp hang

   boot1msg db 'Loaded into bootloader1', 13, 10, 0 ; label pointing to address of message + CR + LF

times 8192-($-$$) db 0 ; fill rest of 8192 bytes with 0s