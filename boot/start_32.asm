; calls cmain()
; used to ensure jumping to kernel_start calls cmain
; sets up videomode and surface_boot in memory

jmp start_32

global start_32
start_32:

global videomode
videomode dd 0
mov [videomode], ecx

global surface_boot
surface_boot dd 0
mov [surface_boot], edx

extern cmain
call cmain