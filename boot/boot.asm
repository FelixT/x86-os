; bootloader 0, load 64kb bootloader1 into memory at 0x7e00 and jump

[ORG 0x7c00]
   jmp start

   %include "boot/print.asm"

start:

   ; clear screen
   mov ax, 0xB800
   mov es, ax
   xor di, di
   mov cx, 2000
   mov ah, 0x07
   mov al, ' '
   rep stosw

   ; set cursor to top left
   mov ah, 0x02
   mov bh, 0x00
   xor dx, dx
   int 0x10

   mov ax, 0
   mov ds, ax ; data segment=0
   mov es, ax
   mov ss, ax ; stack starts at 0
   mov sp, 0x5c00 ; stack pointer starts 0x2000 before code - so as to not overwrite anything
   cld ; clear direction flag

   ; enable A20
   mov ax, 0x2401
   int 0x15

   mov si, boot0msg
   call print

   ; load straight into bootloader1 as this is limited to just 512 bytes
   call read_kernel ; only returns if the read failed, with the BIOS error code in ah

   push ax
   mov si, errormsg
   call print
   pop ax
   mov al, ah
   call print_hex8

   jmp $ ; infinite loop

read_kernel:
   mov si, boot1loadmsg
   call print

   mov dl, 0x80 ; read from hard drive
   mov ah, 0x02 ; 'read sectors from drive'
   mov al, 127 ; number of sectors to read: 127=63.5KiB
   mov ch, 0 ; cyclinder no
   mov cl, 2 ; sector no [starts at 1]
   mov dh, 0 ; head no
   mov bx, 0 ; set es:bx
   mov es, bx ; first part of memory pointer (should be 0)
   mov bx, 0x7e00 ; 512 after our bootloader start
   int 0x13
   jc .fail ; carry set = read failed, error code in ah

   jmp 0x7e00

   .fail:
      ret

; print al as two hex digits
print_hex8:
   push ax
   shr al, 4 ; high nibble
   call .nibble
   pop ax
   ; fall through for low nibble
   .nibble:
      and al, 0x0F
      add al, '0'
      cmp al, '9'
      jbe .out
      add al, 7 ; skip from '9' to 'A'
   .out:
      jmp print_ch ; tail call, print_ch returns to our caller

   ; strings
   boot0msg db 'Starting bootloader0', 13, 10, 0 ; label pointing to address of message + CR + LF
   boot1loadmsg db 'Loading bootloader1 from disk', 13, 10, 0 ; label pointing to address of message + CR + LF
   errormsg db 'Unable to load bootloader1, BIOS error 0x', 0

   times 446-($-$$) db 0 ; code must end before the partition table - nasm errors if it doesn't
   times 64 db 0 ; partition table, left empty so ata_check_mbr treats this image as the whole disk
   dw 0xAA55 ; marker to show we're a bootloader to some BIOSes