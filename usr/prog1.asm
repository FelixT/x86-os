; todo: remove
[bits 32]

; disable term draw func
mov eax, 11
mov ebx, 5 ; WM_DRAW
mov ecx, 0
mov edx, -1
int 0x30

; set title
mov eax, 42
mov ebx, title_str
mov ecx, -1
int 0x30

infloop:

mov edx, 0 ; counter (unused reg)

.loop:

mov eax, 1 ; print 42
mov ebx, out_str
mov ecx, -1
int 0x30

inc edx
cmp edx, 4
jl .loop ; edx < 4

; redraw
mov eax, 9
mov ebx, -1
int 0x30

; sleep
mov eax, 31
mov ebx, 30 ; ms
int 0x30

jmp infloop

ret

out_str db '42', 0
title_str db 'prog1.bin', 0

times 512-($-$$) db 0
