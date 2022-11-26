[bits 32]

;section .text
;global _start

;_start:

; display note at program start
push eax
push ebx
mov eax, 1
mov ebx, msg
int 0x30
pop ebx
pop eax

loop:
jmp loop

;section .data

msg db 'This is the system idle process', 10, 0
