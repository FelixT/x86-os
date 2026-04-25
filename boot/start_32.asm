; calls cmain()
; used to ensure jumping to kernel_start calls cmain
; sets up videomode and surface_boot in memory

jmp start_32

global start_32
start_32:
   mov [videomode], ecx
   mov [surface_boot], edx
   extern cmain
   call cmain

global videomode
videomode dd 0

global surface_boot
surface_boot dd 0