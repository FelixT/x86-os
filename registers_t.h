#ifndef REGISTERS_T_H
#define REGISTERS_T_H

// general registers in order they are pushed onto stack
// https://faydoc.tripod.com/cpu/pusha.htm
typedef struct registers_t {
   uint32_t ds;
   uint32_t edi; // saved with pusha
   uint32_t esi;
   uint32_t ebp;
   uint32_t esp; // ignored by popa
   uint32_t ebx;
   uint32_t edx;
   uint32_t ecx;
   uint32_t eax;
   uint32_t err_code; // or dummy
   uint32_t eip, cs, eflags, useresp, ss; // automatically pushed upon interrupt
} __attribute__((packed)) registers_t;

#endif