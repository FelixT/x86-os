vbe_info_structure:
	.signature		db "VBE2"	; indicate support for VBE 2.0+
	.table_data:	times 512-4 db 0	; reserve space for the table below

global vbe_mode_info_structure
vbe_mode_info_structure:
	.table_data:	times 256 db 0	; reserve space for the table below

vesa_init:
   ; get vesa modes https://wiki.osdev.org/User:Omarrx024/VESA_Tutorial
   mov ax, 0x4F00
   mov bx, 0
   mov es, bx
   mov di, vbe_info_structure
   
   push es
   int 0x10
   pop es

   ; not supported
   cmp ax, 0x004F
   jne .gui_error

   ; get video_modes
   mov di, word[vbe_info_structure+14]
   mov es, word[vbe_info_structure+16]

   ; pick best mode
   .video_modes_di dw 0
   .best_width dw 0
   .best_entry dw 0
   .offset dw 0 ; i = 0

   mov [.video_modes_di], di

   ;mov cx, 0 ; best entry
   ;mov dx, 0 ; best width

   mov bx, 0 ; i = 0

   .loop:
      push bx
      mov bh, 0 ; page number
      int 0x10
      pop bx

      ; get mode info
      ;push cx ; save best entry

      ; get mode as cx
      ;push di ; save di
      mov di, word[.video_modes_di]
      add di, bx
      add di, bx
      mov cx, [es:di] ; es:(.video_modes_di+bx*2)
      ;pop di
      cmp cx, 0xFFFF ; reached end of array
      je .loop_done

      ; populate vbe_mode_info_structure with mode=cx
      ;push di
      mov ax, 0x4F01
      mov di, vbe_mode_info_structure
      push es
      int 0x10
      pop es
      ;pop di

      ;pop cx

      cmp ax, 0x004F ; mode unsupported
      jne .loop_next

      ; get width
      mov ax, word[vbe_mode_info_structure+18]
      mov dx, word[.best_width]
      cmp dx, ax
      jnl .loop_next ; less than or equal to best, discard

      ; if width too big, discard...
      cmp ax, 1100
      jnl .loop_next

      ; get bpp
      mov ah, byte[vbe_mode_info_structure+25]
      cmp ah, 16
      jne .loop_next ; use bpp 16 for now... if not 16, discard

      ; get framebuffer status, check 7th bit
      mov ax, word[vbe_mode_info_structure]
      and ax, 0x80
      cmp ax, 0x80 ; supports linear frame buffer
      jne .loop_next ; otherwise discard

      ; otherwise, this is our new best width & entry
      mov di, word[.video_modes_di]
      add di, bx
      add di, bx
      mov dx, word[vbe_mode_info_structure+18] ; save width
      mov [.best_width], dx
      ;mov ax, bx
      ;add ax, ax
      mov cx, [es:di] ; save mode
      mov [.best_entry], cx
      ;pop di
      
      jmp .loop_next

   .loop_next:
      inc bx
      jmp .loop

   .loop_done:
      ; get best entry
      mov cx, word[.best_entry]
      cmp cx, 0
      je .gui_error

      ; get info for this specific mode
      mov ax, 0x4F01
      mov cx, [.best_entry]
      mov di, vbe_mode_info_structure
      push es
      int 0x10
      pop es

      ; set vga mode
      mov ax, 0x4F02	; set VBE mode
      mov bx, cx
      or bx, 0x4000 ; set bit 14 of bx to enable linear framebuffer
      push es
      int 0x10
      pop es

      cmp ax, 0x004F
      jne .gui_error

      ret

   .gui_error:
      ; revert to text mode 
      mov ah, 0x00 ; set video mode
      mov al, 0x03 ; video mode = 80x25 16 color text vga
      int 0x10

      ; revert to 320x200
      ;mov ah, 0x00 ; set video mode
      ;mov al, 0x13 ; video mode = 80x25 16 color text vga
      ;int 0x10

      mov si, error
      call print

      ret
