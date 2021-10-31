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

terminal_newline:
   mov si, prompt
   call print
   ret
; 
terminal_readline:
   ; get cursor position
   mov ah, 0x03
   int 0x10

   ; set x to 1
   mov dl, 1
   mov ah, 0x02
   int 0x10

   ; read characters until recieving a null
   mov bx, 0 ; index
   .read:
      mov ah, 0x08
      int 0x10 ; returns character as al

      cmp al, 0x20
      jz .done

      ; write to memory
      mov byte [0x6000+bx], al

      add bx, 1

      ; move cursor to right
      mov ah, 0x03
      int 0x10
      add dl, 1
      mov ah, 0x02
      int 0x10

      jmp .read

   .done:
      mov al, 0
      mov byte [0x6000+bx], al
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
      ; get current cursor position: AX = 0, CH = Start scan line, CL = End scan line, DH = Row, DL = Column
      mov ah, 0x03
      int 0x10

      ; read line
      call terminal_readline

      mov al, 0x0d ; print newline char
      call print_ch ; print carriage return char

      mov al, 0x0a ; print newline char
      call print_ch

      ; print line inputted
      mov si, 0x6000
      call print
      
      mov al, 0x0d ; print newline char
      call print_ch ; print carriage return char

      mov al, 0x0a ; print newline char
      call print_ch

      call terminal_newline

      jmp .done
      ;jmp read_kernel

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

prompt db '>', 0