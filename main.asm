[bits 16]

section .text

jmp kmain

%include "terminal.asm"
%include "terminal_plus.asm"
%include "interrupts.asm"
%include "gui.asm"
%include "gdt.asm"

extern tos_kernel

global kmain
kmain:
   mov sp, tos_kernel

   ;extern terminal_clear
   ;call terminal_clear

   mov si, boot1msg
   call print

   call setup_interrupts

   call terminal_newline

   .loop:

      ; if cli
      mov ah, 0x0f
      int 0x10
      mov ah, 0
      cmp ax, 0x03 ; mode = 3
      jz .cli
      jmp .gui ; else if gui

      .cli:
         jmp .loop

      .gui:
         call drawrect
         call drawmouse
         
         jmp .loop

videoinfo:
   ; get videoinfo as al
   mov ah, 0x0f
   int 0x10
   mov ah, 0

   ; print video info
   call print_num

   ; print CR LF
   mov al, 13
   call print_ch
   mov al, 10
   call print_ch

   ret

check_cmd:
   ; 'gui'
   mov di, cmd_gui
   call compare_strings
   cmp ax, 1
   jz .cmd_gui

   ; 'videoinfo'
   mov di, cmd_videoinfo
   call compare_strings
   cmp ax, 1
   jz .cmd_videoinfo

   ; 'mouse'
   mov di, cmd_mouse
   call compare_strings
   cmp ax, 1
   jz .cmd_mouse

   ; 'mouseinfo'
   mov di, cmd_mouseinfo
   call compare_strings
   cmp ax, 1
   jz .cmd_mouseinfo

   ; 'protected'
   mov di, cmd_protected
   call compare_strings
   cmp ax, 1
   jz .cmd_protected

   ; 'protectedgui'
   mov di, cmd_protectedgui
   call compare_strings
   cmp ax, 1
   jz .cmd_protectedgui

   jmp .done

   .cmd_gui:
      call load_gui
      jmp .done

   .cmd_videoinfo:
      call videoinfo
      jmp .done
   
   .cmd_mouse:
      call mouse_enable
      jmp .done

   .cmd_mouseinfo:
      call mouse_info
      jmp .done

   .cmd_protected:
      ; enter protected mode
      cli ; disable interrupts
      lgdt [gdt_descriptor] ; load GDT register with start address of Global Descriptor Table
      mov eax, cr0
      or eax, 0x1
      mov cr0, eax
      jmp CODE_SEG:.main_32

      [bits 32]

      %include "main_32.asm"

      [bits 16]

   .cmd_protectedgui:

      ; get vesa modes https://wiki.osdev.org/User:Omarrx024/VESA_Tutorial
      mov ax, 0x4F00
      mov bx, 0
      mov es, bx
      mov di, vbe_info_structure
      int 0x10

      cmp ax, 0x004F
      jne .gui_error

      
      ; get video_modes
      mov di, [vbe_info_structure+14]
      mov es, [vbe_info_structure+16]

      ; pick best mode
      mov cx, 0 ; best entry
      mov dx, 0 ; best width

      mov bx, 0 ; i = 0
      .loop:
         ; get mode info
         push cx
         push di
         add di, bx
         add di, bx
         mov cx, [es:di]
         pop di
         cmp cx, 0xFFFF ; reached end of array
         je .loop_done

         push di
         mov ax, 0x4F01
         mov di, vbe_mode_info_structure
         int 0x10
         pop di

         pop cx

         cmp ax, 0x004F ; mode unsupported
         jne .loop_next

         ; get width
         mov ax, [vbe_mode_info_structure+18]
         cmp dx, ax
         jnl .loop_next ; less than or equal to best, discard

         ; if width too big, discard...
         cmp ax, 1000
         jnl .loop_next

         ; get bpp
         mov ah, [vbe_mode_info_structure+25]
         cmp ah, 8
         jne .loop_next ; only use bpp 8 for now...

         ; get framebuffer status, check 7th bit
         mov ax, [vbe_mode_info_structure]
         and ax, 0x80
         cmp ax, 0x80 ; supports linear frame buffer
         jne .loop_next ; otherwise discard

         ; otherwise, this is our new best width & entry
         push di
         add di, bx
         add di, bx
         mov dx, [vbe_mode_info_structure+18]
         mov ax, bx
         add ax, ax
         mov cx, [es:di]
         pop di
         
         jmp .loop_next

      .loop_next:
         inc bx
         jmp .loop

      .loop_done:
         pop cx

         cmp cx, 0
         je .gui_error

         ; get info for this specific mode cx
         mov ax, 0x4F01
         mov di, vbe_mode_info_structure
         int 0x10

         ; set vga mode
         mov ax, 0x4F02	; set VBE mode
         mov bx, cx
         or bx, 0x4000 ; set bit 14 of bx to enable linear framebuffer
         int 0x10

         cmp ax, 0x004F
         jne .gui_error
         
      ; enter protected mode
      cli ; disable interrupts
      lgdt [gdt_descriptor] ; load GDT register with start address of Global Descriptor Table
      mov eax, cr0
      or eax, 0x1
      mov cr0, eax
      jmp CODE_SEG:.maingui_32

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

         jmp .done

   .done:
      ret

load_gui:
   call gui_enable

  ; extern gui_draw
   ;call gui_draw

   call drawrect

   ret

; https://stackoverflow.com/questions/54280828/making-a-mouse-handler-in-x86-assembly
ARG_OFFSETS      equ 6          ; Offset of args from BP

mouse_handler:

   push bp                     ; Function prologue
   mov bp, sp
   push ds                     ; Save registers we modify
   push ax
   push bx
   push cx
   push dx

   push cs
   pop ds                      ; DS = CS, CS = where our variables are stored

   mov al,[bp+ARG_OFFSETS+6]
   mov bl, al                  ; BX = copy of status byte
   mov cl, 3                   ; Shift signY (bit 5) left 3 bits
   shl al, cl                  ; CF = signY
                              ; Sign bit of AL = SignX
   sbb dh, dh                  ; CH = SignY value set in all bits
   cbw                         ; AH = SignX value set in all bits
   mov dl, [bp+ARG_OFFSETS+2]  ; CX = movementY
   mov al, [bp+ARG_OFFSETS+4]  ; AX = movementX

   ; new mouse X_coord = X_Coord + movementX
   ; new mouse Y_coord = Y_Coord + (-movementY)
   neg dx
   mov cx, [mouseY]
   add dx, cx                  ; DX = new mouse Y_coord
   mov cx, [mouseX]
   add ax, cx                  ; AX = new mouse X_coord

   ; Status
   mov [mouseX], ax            ; Update current virtual mouseX coord
   mov [mouseY], dx            ; Update current virtual mouseY coord

   pop dx                      ; Restore all modified registers
   pop cx
   pop bx
   pop ax
   pop ds
   pop bp                      ; Function epilogue

   retf

mouse_info:
   ; print mouse x & y
   mov ax, [mouseX]
   call print_num

   mov al, 44
   call print_ch

   mov ax, [mouseY]
   call print_num

mouse_enable:
   ; https://www.ctyme.com/intr/int-15.htm : Int 15/AX=00h - 209h

   mov ax, 0xc205 ; init
   mov bh, 3 ; package size
   int 0x15
   
   mov al, ah ; print status
   mov ah, 0
   call print_num

   mov ax, 0xC203              ; Set resolution
   mov bh, 3    ; 8 counts / mm
   int 0x15                    ; Call BIOS to set resolution

   ; setup callback
   push cs
   pop es
   mov bx, mouse_handler
   mov ax, 0xc207
   int 0x15

   mov ax, 0xc200 ; enable
   mov bh, 1
   int 0x15

   mov al, ah ; print status
   mov ah, 0
   call print_num

   ret

section .data
; strings
boot1msg db 'Loaded into bootloader1', 13, 10, 0 ; label pointing to address of message + CR + LF
error db 'Error!', 13, 10, 0
cmd_gui db 'gui', 0
cmd_videoinfo db 'videoinfo', 0
cmd_mouse db 'mouse', 0
cmd_mouseinfo db 'mouseinfo', 0
cmd_protected db 'protected', 0
cmd_protectedgui db 'protectedgui', 0

vbe_info_structure:
	.signature		db "VBE2"	; indicate support for VBE 2.0+
	.table_data:	resb 512-4	; reserve space for the table below

global vbe_mode_info_structure
vbe_mode_info_structure:
	.table_data:	resb 256	; reserve space for the table below

%include "font.asm"

;times 32768-($-$$) db 0 ; fill rest of 32768 bytes with 0s