#include <stdint.h>
#include "../windowobj.h"

#define NULL ( (void *) 0)

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

static inline void redraw_pixel(int x, int y) {
   asm volatile(
      "int $0x30"
      :: "a" (37),
      "b" ((uint32_t)x),
      "c" ((uint32_t)y)
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

static inline void override_draw(uint32_t addr) {
   asm volatile(
      "int $0x30"
      :: "a" (29),
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

static inline uint32_t *malloc(uint32_t size) {
   uint32_t addr;

   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (addr)
      : "a" (16),
      "b" (size)
   );

   return (uint32_t*)addr;
}

static inline void free(uint32_t addr, uint32_t size) {
   asm volatile (
      "int $0x30;"
      :: "a" (40),
      "b" (addr),
      "c" (size)
   );
}

static inline uint32_t *fat_get_bpb() {
   uint32_t addr;

   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (addr)
      : "a" (17)
   );

   return (uint32_t*)addr;
}

static inline uint32_t *fat_read_root() {
   uint32_t addr;

   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (addr)
      : "a" (18)
   );

   return (uint32_t*)addr;
}

static inline uint32_t *fat_parse_path(char *path, bool isfile) {
   uint32_t addr;

   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (addr)
      : "a" (19),
      "b" ((uint32_t)path),
      "c" ((uint32_t)isfile)
   );

   return (uint32_t*)addr;
}

static inline uint32_t *fat_read_file(uint16_t firstClusterNo, uint32_t fileSize) {
   uint32_t addr;

   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (addr)
      : "a" (20),
      "b" ((uint32_t)firstClusterNo),
      "c" (fileSize)
   );

   return (uint32_t*)addr;
}

static inline void fat_write_file(char *path, uint8_t *buffer, uint32_t size) {
   asm volatile (
      "int $0x30;"
      :: "a" (33),
      "b" ((uint32_t)path),
      "c" ((uint32_t)buffer),
      "d" ((uint32_t)size)
   );
}

static inline void fat_new_file(char *path) {
   asm volatile (
      "int $0x30;"
      :: "a" (41),
      "b" ((uint32_t)path)
   );
}

static inline void bmp_draw(uint8_t *bmp, int x, int y, int scale) {
   asm volatile (
      "int $0x30;"
      :: "a" (21),
      "b" ((uint32_t)bmp),
      "c" ((uint32_t)x),
      "d" ((uint32_t)y),
      "S" ((uint32_t)scale)
   );
}

static inline void write_strat(char *str, int x, int y) {
   asm volatile(
      "int $0x30"
      :: "a" (22),
      "b" ((uint32_t)str),
      "c" ((uint32_t)x),
      "d" ((uint32_t)y)
   );
}

static inline void clear() {
   asm volatile(
      "int $0x30"
      :: "a" (23));
}

static inline int fat_get_dir_size(int firstClusterNo) {
   uint32_t output;

   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (output)
      : "a" (24),
      "b" ((uint32_t)firstClusterNo)
   );

   return output;
}

static inline uint32_t *fat_read_dir(int firstClusterNo) {
   uint32_t addr;

   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (addr)
      : "a" (25),
      "b" ((uint32_t)firstClusterNo)
   );

   return (uint32_t*)addr;
}

static inline void debug_write_uint(int num) {
   asm volatile(
      "int $0x30"
      :: "a" (26),
      "b" (num)
   );
}

static inline void override_downarrow(uint32_t addr) {
   asm volatile(
      "int $0x30"
      :: "a" (27),
      "b" (addr)
   );
}

static inline void override_resize(uint32_t addr) {
   asm volatile(
      "int $0x30"
      :: "a" (34),
      "b" (addr)
   );
}

static inline void override_drag(uint32_t addr) {
   asm volatile(
      "int $0x30"
      :: "a" (36),
      "b" (addr)
   );
}

static inline void override_mouserelease(uint32_t addr) {
   asm volatile(
      "int $0x30"
      :: "a" (38),
      "b" (addr)
   );
}

static inline void write_numat(int num, int x, int y) {
   asm volatile(
      "int $0x30"
      :: "a" (28),
      "b" ((uint32_t)num),
      "c" ((uint32_t)x),
      "d" ((uint32_t)y)
   );
}

static inline void queue_event(uint32_t callback, int delta) {
   asm volatile(
      "int $0x30"
      :: "a" (30),
      "b" ((uint32_t)callback),
      "c" ((uint32_t)delta)
   );
}

static inline windowobj_t *register_windowobj(int type, int x, int y, int width, int height) {
   uint32_t addr;
    asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (addr)
      : "a" (31)
   );
   windowobj_t *wo = (windowobj_t*)addr;
   wo->type = type;
   wo->x = x;
   wo->y = y;
   wo->width = width;
   wo->height = height;
   if(type != WO_BUTTON) {
      wo->texthalign = false;
      wo->textvalign = false;
   }
   return wo;
}

static inline void launch_task(char *path, int argc, char **args) {
   asm volatile (
      "int $0x30;"
      :: "a" (32),
      "b" ((uint32_t)path),
      "c" ((uint32_t)argc),
      "d" ((uint32_t)args)
   );
}

static inline void set_sys_font(char *path) {
   asm volatile (
      "int $0x30;"
      :: "a" (35),
      "b" ((uint32_t)path)
   );
}

// terminal override

static inline void override_term_checkcmd(uint32_t addr) {
   asm volatile (
      "int $0x30;"
      :: "a" (39),
      "b" ((uint32_t)addr)
   );
}