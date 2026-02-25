[bits 32]

;section .text
;global _start

;_start:

; display note at program start
push eax
push ebx
mov eax, 1
mov ebx, msg
mov ecx, -1
int 0x30
pop ebx
pop eax

; yield loop
loop:
mov eax, 3
int 0x30
jmp loop

;section .data

msg db 'This is the system idle process', 10, 0
