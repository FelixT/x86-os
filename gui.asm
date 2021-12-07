; https://stanislavs.org/helppc/int_10-0.html

gui_enable:
   mov ah, 0x00 ; set video mode
   mov al, 0x13 ; video mode = 320x200 256 color graphics 
   int 0x10
   ret

gui_disable:
   mov ah, 0x00 ; set video mode
   mov al, 0x03 ; video mode = 80x25 16 color text vga
   int 0x10
   ret

setpixel:
   ; AH = colour
   ; DI = pixel position/buffer offset
   mov ebx, 0xA0000
   mov bh, 200 ; offset
   mov ax, 0xA000
   mov gs, ax
   mov [gs:di], ah ; set pixel buffet at pos edi
   ret

drawrect:
   mov ebx, 0xA0000
   mov bh, 200 ; offset
   mov ax, 0xA000
   mov gs, ax

   mov cx, 0 ; y = 0
   .for_y:
      
      mov bx, 0 ; x = 0
      .for_x:

         ; plot pixel
         mov ah, 4 ; pixel colour
         mov di, cx
         imul di, 320 ; width

         mov [gs:di+bx], ah ; y*width + x

         inc bx ; x++
         cmp bx, 320
         jl .for_x ; x < 10

      inc cx ; y++
      cmp cx, 15
      jl .for_y ; y < 3
   ret
