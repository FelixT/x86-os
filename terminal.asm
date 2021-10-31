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

terminal_keypress:
   ; takes key as ascii character 
   cmp al, 0x08 ; if key is backspace
   jz .backspace

   call print_ch

   cmp al, 0x0d ; if key is return
   jz .return

   ret

   .return:
      mov al, 0x0a ; print newline char
      call print_ch
      jmp read_kernel

   .backspace:
      mov al, 'b' ; print newline char
      call print_ch

      ret