; https://wiki.osdev.org/Interrupts_tutorial
; https://wiki.osdev.org/Interrupt_Service_Routines

[bits 32]

%macro isr_err_stub 1
isr_stub_%+%1:
   pusha

   mov ax, ds ; save data segment in lower 16 bits of eax
   push dword eax

   push dword esp ; push stackpointer (points to register saved by pusha, used as pointer to registers_t struct)
   push dword %1 ; push int no

   cld 
   call err_exception_handler

   add esp, 4 ; pop int no
   add esp, 4 ; pop stack pointer

   pop eax ; restore data segment
   mov ds, ax
   mov es, ax
   mov fs, ax
   mov gs, ax

   popa
   add esp, 4 ; extra pop for err code
   iret
%endmacro

%macro isr_no_err_stub 1
isr_stub_%+%1:
   push dword 0 ; dummy err code
   pusha

   mov ax, ds ; save data segment in lower 16 bits of eax
   push dword eax

   push dword esp ; push stackpointer (points to register saved by pusha, used as pointer to registers_t struct)
   push dword %1 ; push int no

   cld 
   call exception_handler

   add esp, 4 ; pop int no
   add esp, 4 ; pop stack pointer

   pop eax ; restore data segment
   mov ds, ax
   mov es, ax
   mov fs, ax
   mov gs, ax

   popa
   add esp, 4 ; extra pop for err code
   iret
%endmacro

%macro irq_stub 1
irq_stub_%+%1:
   push dword 0 ; dummy err code
   pusha

   mov ax, ds ; save data segment in lower 16 bits of eax
   push dword eax

   push dword esp ; push stackpointer (points to register saved by pusha, used as pointer to registers_t struct)
   push dword %1 ; push int no

   cld 
   call exception_handler

   add esp, 4 ; pop int no
   add esp, 4 ; pop stack pointer

   pop eax ; restore data segment
   mov ds, ax
   mov es, ax
   mov fs, ax
   mov gs, ax

   popa
   add esp, 4 ; extra pop for err code
   iret
%endmacro

extern err_exception_handler
extern exception_handler
isr_no_err_stub 0
isr_no_err_stub 1
isr_no_err_stub 2
isr_no_err_stub 3
isr_no_err_stub 4
isr_no_err_stub 5
isr_no_err_stub 6
isr_no_err_stub 7
isr_err_stub    8
isr_no_err_stub 9
isr_err_stub    10
isr_err_stub    11
isr_err_stub    12
isr_err_stub    13
isr_err_stub    14
isr_no_err_stub 15
isr_no_err_stub 16
isr_err_stub    17
isr_no_err_stub 18
isr_no_err_stub 19
isr_no_err_stub 20
isr_no_err_stub 21
isr_no_err_stub 22
isr_no_err_stub 23
isr_no_err_stub 24
isr_no_err_stub 25
isr_no_err_stub 26
isr_no_err_stub 27
isr_no_err_stub 28
isr_no_err_stub 29
isr_err_stub    30
isr_no_err_stub 31

irq_stub 32
irq_stub 33
irq_stub 34
irq_stub 35
irq_stub 36
irq_stub 37
irq_stub 38
irq_stub 39
irq_stub 40
irq_stub 41
irq_stub 42
irq_stub 43
irq_stub 44
irq_stub 45
irq_stub 46
irq_stub 47
irq_stub 48 ; 0x30 used for software interrupts

global isr_stub_table
isr_stub_table:
%assign i 0 
%rep 32 
   dd isr_stub_%+i
%assign i i+1 
%endrep

global irq_stub_table
irq_stub_table:
%assign i 32
%rep 17 
   dd irq_stub_%+i
%assign i i+1 
%endrep

