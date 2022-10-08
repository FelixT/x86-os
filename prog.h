#include <stdint.h>

static inline void write_str(char *str) {
   asm volatile(
      "int $0x30"
      :: "a" (1),
      "b" ((uint32_t)str)
   );
}

static inline void write_num(int num) {
   asm volatile(
      "int $0x30"
      :: "a" (2),
      "b" (num)
   );
}

static inline void yield() {
   asm volatile(
      "int $0x30"
      :: "a" (3)
   );
}

static inline void write_uint(uint32_t num) {
   asm volatile(
      "int $0x30"
      :: "a" (6),
      "b" (num)
   );
}

static inline uint32_t get_framebuffer() {
   uint32_t output;

   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (output)
      : "a" (7)
   );

   return output;
}

static inline void write_newline() {
   asm volatile(
      "int $0x30"
      :: "a" (8)
   );
}

static inline void redraw() {
   asm volatile(
      "int $0x30"
      :: "a" (9)
   );
}

static inline void exit(int status) {
   asm volatile(
      "int $0x30"
      :: "a" (10),
      "b" (status)
   );
}

static inline void override_uparrow(uint32_t addr) {
   asm volatile(
      "int $0x30"
      :: "a" (11),
      "b" (addr)
   );
}

static inline void end_subroutine() {
   asm volatile(
      "int $0x30"
      :: "a" (12)
   );
}

static inline void override_click(uint32_t addr) {
   asm volatile(
      "int $0x30"
      :: "a" (13),
      "b" (addr)
   );
}

static inline int get_width() {
   uint32_t output;

   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (output)
      : "a" (14)
   );

   return output;
}

static inline int get_height() {
   uint32_t output;

   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (output)
      : "a" (15)
   );

   return output;
}

static inline uint32_t *malloc() {
   uint32_t addr;

   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (addr)
      : "a" (16)
   );

   return (uint32_t*)addr;
}