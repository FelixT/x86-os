#ifndef PROG_H
#define PROG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "../surface_t.h"

static inline uint16_t rgb16(uint8_t r, uint8_t g, uint8_t b) {
   // 5r 6g 5b
   return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

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

static inline surface_t get_surface(void) {
    surface_t surface;
    asm volatile (
        "int $0x30"
        : "=b" (surface.buffer), "=c" (surface.width), "=d" (surface.height)
        : "a" (7)
    );
    return surface;
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

static inline void *malloc(uint32_t size) {
   uint32_t addr;

   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (addr)
      : "a" (16),
      "b" (size)
   );

   return (void*)addr;
}

static inline void free(void *addr, uint32_t size) {
   asm volatile (
      "int $0x30;"
      :: "a" (40),
      "b" ((uint32_t*)addr),
      "c" (size)
   );
}

static inline bool mkdir(char *path) {
   int success;
   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (success)
      : "a" (50),
      "b" ((uint32_t)path)
   );
   return (bool)success;
}

static inline void bmp_draw(uint8_t *bmp, int x, int y, int scale, bool white_is_transparent) {
   asm volatile (
      "int $0x30;"
      :: "a" (21),
      "b" ((uint32_t)bmp),
      "c" ((uint32_t)x),
      "d" ((uint32_t)y),
      "S" ((uint32_t)scale),
      "D" ((uint32_t)white_is_transparent)
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

// from fs.h
#define FS_MAX_FILENAME 255

#define FS_TYPE_FILE 0
#define FS_TYPE_DIR 1
#define FS_TYPE_TERM 2

typedef struct {
   char filename[FS_MAX_FILENAME];
   int type;
   uint32_t file_size;
   bool hidden;
} fs_dir_entry_t;

typedef struct {
   fs_dir_entry_t *entries;
   int size;
} fs_dir_content_t;

static inline fs_dir_content_t *read_dir(char *path) {
   uint32_t addr;

   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (addr)
      : "a" (25),
      "b" ((uint32_t)path)
   );

   return (fs_dir_content_t*)addr;
}

static inline void debug_write_str(char *str) {
   asm volatile(
      "int $0x30"
      :: "a" (49),
      "b" (str)
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

static inline void launch_task(char *path, int argc, char **args, bool copy) {
   asm volatile (
      "int $0x30;"
      :: "a" (32),
      "b" ((uint32_t)path),
      "c" ((uint32_t)argc),
      "d" ((uint32_t)args),
      "S" ((uint32_t)copy) // inherit fds and wd from current task
   );
}

static inline void set_sys_font(char *path) {
   asm volatile (
      "int $0x30;"
      :: "a" (35),
      "b" ((uint32_t)path)
   );
}

static inline void set_window_title(char *title) {
   asm volatile (
      "int $0x30;"
      :: "a" (42),
      "b" ((uint32_t)title)
   );
}

static inline void chdir(char *path) {
   asm volatile (
      "int $0x30;"
      :: "a" (43),
      "b" ((uint32_t)path)
   );
}

static inline void getwd(char *buf) {
   asm volatile (
      "int $0x30;"
      :: "a" (44),
      "b" ((uint32_t)buf)
   );
}

static inline void display_popup(char *title, char *message, bool output, void* callback) {
   asm volatile (
      "int $0x30;"
      :: "a" (45),
      "b" ((uint32_t)title),
      "c" ((uint32_t)message),
      "d" ((uint32_t)output),
      "S" ((uint32_t)callback)
   );
}

static inline void display_colourpicker(uint16_t colour, void *callback) {
   asm volatile (
      "int $0x30;"
      :: "a" (46),
      "b" ((uint32_t)colour),
      "c" ((uint32_t)callback)
   );
}

static inline void display_filepicker(void *callback) {
   asm volatile (
      "int $0x30;"
      :: "a" (48),
      "b" ((uint32_t)callback)
   );
}

static inline int open(char *path, int flag) {
   int fd;
   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (fd)
      : "a" (52),
      "b" ((uint32_t)path),
      "c" (flag)
   );
   return fd;
}

static inline int fsize(int fd) {
   int size;
   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (size)
      : "a" (59),
      "b" ((uint32_t)fd)
   );
   return size;
}

static inline int read(int fd, char *buf, size_t count) {
   int c;
   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (c)
      : "a" (47),
      "b" ((uint32_t)fd),
      "c" ((uint32_t)buf),
      "d" ((uint32_t)count)
   );
   return c;
}

static inline int write(int fd, char *buf, size_t count) {
   int c;
   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (c)
      : "a" (53),
      "b" ((uint32_t)fd),
      "c" ((uint32_t)buf),
      "d" ((uint32_t)count)
   );
   return c;
}

static inline int new_file(char *path) {
   int fd;
   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (fd)
      : "a" (41),
      "b" ((uint32_t)path)
   );
   return fd;
}

static inline int close(int fd) {
   (void)fd;
   // do nothing yet
   return 0;
}

static inline bool rename(char *path, char *newname) {
   int success;
   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (success)
      : "a" (58),
      "b" ((uint32_t)path),
      "c" ((uint32_t)newname)
   );
   return (bool)success;
}

static inline uint32_t *sbrk(uint32_t increment) {
   uint32_t addr;

   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (addr)
      : "a" (51),
      "b" ((uint32_t)increment)
   );

   return (void*)addr;
}

static inline void create_scrollbar(void (*callback)(int deltaY, int offsetY)) {
   asm volatile (
      "int $0x30"
      :: "a" (54),
      "b" ((uint32_t)callback)
   );
}

static inline uint32_t set_content_height(uint32_t height) {
   uint32_t width;
   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (width)
      : "a" (55),
      "b" (height)
   );

   return width;
}

static inline void scroll_to(uint32_t y) {
   asm volatile (
      "int $0x30"
      :: "a" (56),
      "b" (y)
   );
}

static inline void set_window_size(int width, int height) {
      asm volatile (
      "int $0x30"
      :: "a" (60),
      "b" (width),
      "c" (height)
   );
}

typedef struct font_info_t {
   int width;
   int height;
} font_info_t;

static inline font_info_t get_font_info() {
   font_info_t info;
      asm volatile (
      "int $0x30"
      : "=b" (info.width), "=c" (info.height)
      : "a" (61)
   );
   return info;
}

// terminal override

static inline void override_term_checkcmd(uint32_t addr) {
   asm volatile (
      "int $0x30;"
      :: "a" (39),
      "b" ((uint32_t)addr)
   );
}

#endif