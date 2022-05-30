[bits 32]

infloop:

mov ecx, 0 ; counter

.loop:

mov eax, 6 ; print 42
mov ebx, 42
mov ecx, 0 ; window 0
int 0x30

inc ecx
cmp ecx, 10
jl .loop ; ecx < 10

; yield
mov eax, 3
int 0x30

jmp infloop

ret

times 512-($-$$) db 0