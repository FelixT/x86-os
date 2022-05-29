[bits 32]

infloop:

mov ecx, 0 ; counter

.loop:

mov eax, 2 ; print 42
mov ebx, 42
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