extern tos_kernel

global tss_start
tss_start:
   dd 0
   dd tos_kernel ; esp0, kernel stack pointer
   dd DATA_SEG ; ss0, kernel stack segment
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0
   dd 0 ; es
   dd 0 ; cs
   dd 0 ; ss
   dd 0 ; ds
   dd 0 ; fs
   dd 0 ; gs
   dd 0
   dw 0
   dw 0

global tss_end
tss_end: