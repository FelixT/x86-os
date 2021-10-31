[ORG 0x7c00]
   jmp start

   %include "terminal.asm"
   %include "interrupts.asm"

start:
   mov ax, 0
   mov ds, ax ; data segment=0
   mov ss, ax ; stack starts at 0
   mov sp, 0x9c00 ; stack pointer starts (0x9c00-0x07c0)=0x2000h past code
   cld ; clear direction flag

   mov si, boot0msg
   call print

   call setup_interrupts

   call terminal_newline
   
   jmp $ ; infinite loop

check_cmd:
   mov di, cmd_load
   call compare_strings
   cmp ax, 1
   jz read_kernel

   ret

read_kernel:
   mov si, boot1loadmsg
   call print

   mov dl, 0x80 ; read from hard drive
   mov ah, 0x02 ; 'read sectors from drive'
   mov al, 16 ; number of sectors to read
   mov ch, 0 ; cyclinder no
   mov cl, 2 ; sector no [starts at 1]
   mov dh, 0 ; head no
   mov bx, 0
   mov es, bx ; first part of memory pointer (should be 0)
   mov bx, 0x7e00 ; 512 after our bootloader start 
   int 0x13
   jmp 7e00h

   ; strings
   boot0msg db 'Starting bootloader0', 13, 10, 0 ; label pointing to address of message + CR + LF
   boot1loadmsg db 'Loading bootloader1 from disk', 13, 10, 0 ; label pointing to address of message + CR + LF
   cmd_load db 'load', 0


   times 510-($-$$) db 0 ; fill rest of 512 bytes with 0s (-2 due to signature below)
   dw 0xAA55 ; marker to show we're a bootloader to some BIOSes