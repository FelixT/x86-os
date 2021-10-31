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

   cmp al, 0x0d ; if key is return
   jz .return

   cmp al, 0x08 ; if key is backspace
   jz .backspace

   call print_ch

   jmp .done

   .return:
      call print_ch ; print carriage return char

      mov al, 0x0a ; print newline char
      call print_ch
      
      jmp read_kernel

   .backspace:
      ; get current cursor position: AX = 0, CH = Start scan line, CL = End scan line, DH = Row, DL = Column
      mov ah, 0x03
      int 0x10

      ; if x == 1 dont allow backspace
      cmp dl, 1
      jz .done

      call print_ch ; print backspace

      mov al, 0x20 ; print delete char
      call print_ch

      ; get current cursor position: AX = 0, CH = Start scan line, CL = End scan line, DH = Row, DL = Column
      mov ah, 0x03
      int 0x10

      sub dl, 1 ; reduce x coordinate

      ; set new cursor position
      mov ah, 0x02
      int 0x10

      jmp .done

   .done:
      ret