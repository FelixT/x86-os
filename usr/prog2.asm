[bits 32]

infloop:

mov ecx, 0 ; counter

.loop:

mov eax, 6 ; print 69
mov ebx, 69
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