#ifndef PROG_H
#define PROG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "../surface_t.h"
#include "../lib/api.h"

static inline uint16_t rgb16(uint8_t r, uint8_t g, uint8_t b) {
   // 5r 6g 5b
   return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

static inline void write_str_w(char *str, int window) {
   asm volatile(
      "int $0x30"
      :: "a" (1),
      "b" ((uint32_t)str),
      "c" (window)
      : "cc", "memory"
   );
}

static inline void write_str(char *str) {
   write_str_w(str, -1);
}

static inline void write_num(int num) {
   asm volatile(
      "int $0x30"
      :: "a" (2),
      "b" (num)
      : "cc", "memory"
   );
}

static inline void yield() {
   asm volatile(
      "int $0x30"
      :: "a" (3)
      : "cc", "memory"
   );
}

static inline void write_uint(uint32_t num) {
   asm volatile(
      "int $0x30"
      :: "a" (6),
      "b" (num)
      : "cc", "memory"
   );
}

static inline surface_t get_surface_w(int window) {
   surface_t surface;
   asm volatile (
      "int $0x30"
      : "=b" (surface.buffer), "=c" (surface.width), "=d" (surface.height)
      : "a" (7),
      "b" (window)
      : "cc", "memory"
   );
   return surface;
}

static inline surface_t get_surface() {
   return get_surface_w(-1);
}

static inline void write_newline() {
   asm volatile(
      "int $0x30"
      :: "a" (8)
      : "cc", "memory"
   );
}

static inline void redraw_w(int w) {
   asm volatile(
      "int $0x30"
      :: "a" (9),
      "b" (w)
      : "cc", "memory"
   );
}

static inline void redraw() {
   redraw_w(-1);
}

static inline void redraw_pixel(int x, int y) {
   asm volatile(
      "int $0x30"
      :: "a" (37),
      "b" ((uint32_t)x),
      "c" ((uint32_t)y)
      : "cc", "memory"
   );
}

static inline void exit(int status) {
   asm volatile(
      "int $0x30"
      :: "a" (10),
      "b" (status)
      : "cc", "memory"
   );
}

static inline void end_subroutine() {
   asm volatile(
      "int $0x30"
      :: "a" (12)
      : "cc", "memory"
   );
}

static inline void override_click(uint32_t addr, int window) {
   asm volatile(
      "int $0x30"
      :: "a" (13),
      "b" (addr),
      "c" (window)
      : "cc", "memory"
   );
}

static inline void override_draw(uint32_t addr, int window) {
   asm volatile(
      "int $0x30"
      :: "a" (29),
      "b" (addr),
      "c" (window)
      : "cc", "memory"
   );
}

static inline int get_width_w(int window) {
   uint32_t output;

   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (output)
      : "a" (14),
      "b" (window)
      : "cc", "memory"
   );

   return output;
}

static inline int get_width() {
   return get_width_w(-1);
}

static inline int get_height_w(int window) {
   uint32_t output;

   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (output)
      : "a" (15),
      "b" (window)
      : "cc", "memory"
   );

   return output;
}

static inline int get_height() {
   return get_height_w(-1);
}

static inline void *kmalloc(uint32_t size) {
   uint32_t addr;

   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (addr)
      : "a" (16),
      "b" (size)
      : "cc", "memory"
   );

   return (void*)addr;
}

static inline void kfree(void *addr, uint32_t size) {
   asm volatile (
      "int $0x30;"
      :: "a" (40),
      "b" ((uint32_t*)addr),
      "c" (size)
      : "cc", "memory"
   );
}

static inline bool mkdir(char *path) {
   int success;
   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (success)
      : "a" (50),
      "b" ((uint32_t)path)
      : "cc", "memory"
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
      : "cc", "memory"
   );
}

static inline void write_strat_w(char *str, int x, int y, int colour, int window) {
   // colour=-1 for window text colour
   asm volatile(
      "int $0x30"
      :: "a" (22),
      "b" ((uint32_t)str),
      "c" ((uint32_t)x),
      "d" ((uint32_t)y),
      "S" ((uint32_t)colour),
      "D" ((uint32_t)window)
      : "cc", "memory"
   );
}

static inline void write_strat(char *str, int x, int y, int colour) {
   write_strat_w(str, x, y, colour, -1);
}

static inline void clear_w(int w) {
   asm volatile(
      "int $0x30"
      :: "a" (23),
      "b" (w)
      : "cc", "memory"
   );
}

static inline void clear() {
   clear_w(-1);
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
      : "cc", "memory"
   );

   return (fs_dir_content_t*)addr;
}

static inline void debug_write_str(char *str) {
   asm volatile(
      "int $0x30"
      :: "a" (49),
      "b" (str)
      : "cc", "memory"
   );
}

static inline void override_resize(uint32_t addr, int window) {
   asm volatile(
      "int $0x30"
      :: "a" (34),
      "b" (addr),
      "c" (window)
      : "cc", "memory"
   );
}

static inline void override_drag(uint32_t addr, int window) {
   asm volatile(
      "int $0x30"
      :: "a" (36),
      "b" (addr),
      "c" (window)
      : "cc", "memory"
   );
}

static inline void override_release(uint32_t addr, int window) {
   asm volatile(
      "int $0x30"
      :: "a" (38),
      "b" (addr),
      "c" (window)
      : "cc", "memory"
   );
}

static inline void override_keypress(uint32_t addr, int window) {
   asm volatile(
      "int $0x30"
      :: "a" (64),
      "b" (addr),
      "c" (window)
      : "cc", "memory"
   );
}

static inline void override_close(uint32_t addr, int window) {
   asm volatile(
      "int $0x30"
      :: "a" (27),
      "b" (addr),
      "c" (window)
      : "cc", "memory"
   );
}

static inline void override_rightclick(uint32_t addr, int window) {
   asm volatile(
      "int $0x30"
      :: "a" (17),
      "b" (addr),
      "c" (window)
      : "cc", "memory"
   );
}

static inline void override_hover(uint32_t addr, int window) {
   asm volatile(
      "int $0x30"
      :: "a" (11),
      "b" (addr),
      "c" (window)
      : "cc", "memory"
   );
}

static inline void override_mouseout(uint32_t addr, int window) {
   asm volatile(
      "int $0x30"
      :: "a" (45),
      "b" (addr),
      "c" (window)
      : "cc", "memory"
   );
}

static inline void write_numat(int num, int x, int y) {
   asm volatile(
      "int $0x30"
      :: "a" (28),
      "b" ((uint32_t)num),
      "c" ((uint32_t)x),
      "d" ((uint32_t)y)
      : "cc", "memory"
   );
}

static inline void queue_event(uint32_t callback, int delta, void *msg) {
   asm volatile(
      "int $0x30"
      :: "a" (30),
      "b" ((uint32_t)callback),
      "c" ((uint32_t)delta),
      "d" ((uint32_t)msg)
      : "cc", "memory"
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
      : "cc", "memory"
   );
}

static inline void end_task(int id) {
   asm volatile (
      "int $0x30;"
      :: "a" (67),
      "b" ((uint32_t)id)
      : "cc", "memory"
   );
}

static inline bool set_setting(api_setting_t setting, uint32_t value) {
   int out;
   asm volatile (
      "int $0x30;"
      : "=b" (out)
      : "a" (35),
      "b" ((uint32_t)setting),
      "c" ((uint32_t)value)
      : "cc", "memory"
   );
   return out == 0;
}

static inline uint32_t get_setting(api_setting_t setting) {
   uint32_t value;
   asm volatile (
      "int $0x30;"
      : "=b" (value)
      : "a" (33),
      "b" ((uint32_t)setting)
      : "cc", "memory"
   );
   return value;
}

static inline bool set_window_setting(int setting, uint32_t value, int window) {
   int out;
   asm volatile (
      "int $0x30;"
      : "=b" (out)
      : "a" (18),
      "b" ((uint32_t)setting),
      "c" ((uint32_t)value),
      "d" ((uint32_t)window)
      : "cc", "memory"
   );
   return out == 0;
}

static inline uint32_t get_window_setting(int setting, int window) {
   uint32_t value;
   asm volatile (
      "int $0x30;"
      : "=b" (value)
      : "a" (19),
      "b" ((uint32_t)setting),
      "c" ((uint32_t)window)
      : "cc", "memory"
   );
   return value;
}

static inline void set_window_title(char *title) {
   asm volatile (
      "int $0x30;"
      :: "a" (42),
      "b" ((uint32_t)title),
      "c" ((uint32_t)-1)
      : "cc", "memory"
   );
}

static inline void set_window_title_w(int window, char *title) {
   asm volatile (
      "int $0x30;"
      :: "a" (42),
      "b" ((uint32_t)title),
      "c" ((uint32_t)window)
      : "cc", "memory"
   );
}

static inline void chdir(char *path) {
   asm volatile (
      "int $0x30;"
      :: "a" (43),
      "b" ((uint32_t)path)
      : "cc", "memory"
   );
}

static inline void getwd(char *buf) {
   asm volatile (
      "int $0x30;"
      :: "a" (44),
      "b" ((uint32_t)buf)
      : "cc", "memory"
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
      : "cc", "memory"
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
      : "cc", "memory"
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
      : "cc", "memory"
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
      : "cc", "memory"
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
      : "cc", "memory"
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
      : "cc", "memory"
   );
   return (bool)success;
}

static inline void *sbrk(uint32_t increment) {
   uint32_t addr;

   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (addr)
      : "a" (51),
      "b" ((uint32_t)increment)
      : "cc", "memory"
   );

   return (void*)addr;
}

static inline void create_scrollbar(void (*callback)(int deltaY, int offsetY, int window), int window) {
   asm volatile (
      "int $0x30"
      :: "a" (54),
      "b" ((uint32_t)callback),
      "c" ((uint32_t)window)
      : "cc", "memory"
   );
}

static inline uint32_t set_content_height(uint32_t height, int window) {
   uint32_t width;
   asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (width)
      : "a" (55),
      "b" (height),
      "c" (window)
      : "cc", "memory"
   );

   return width;
}

static inline void scroll_to(uint32_t y, int window) {
   asm volatile (
      "int $0x30"
      :: "a" (56),
      "b" (y),
      "c" (window)
      : "cc", "memory"
   );
}

static inline void set_window_size(int width, int height) {
      asm volatile (
      "int $0x30"
      :: "a" (60),
      "b" (width),
      "c" (height)
      : "cc", "memory"
   );
}

static inline void set_window_position(int x, int y, int window) {
   asm volatile (
      "int $0x30"
      :: "a" (20),
      "b" (x),
      "c" (y),
      "d" (window)
      : "cc", "memory"
   );
}

typedef struct coord_t {
   int x;
   int y;
} coord_t;

static inline coord_t get_window_position(int window) {
   uint32_t x;
   uint32_t y;
   asm volatile (
      "int $0x30"
      : "=b" (x),
      "=c" (y)
      : "a" (26),
      "b" (window)
      : "cc", "memory"
   );
   coord_t coord = {x,y};
   return coord;
}

static inline void set_window_minimised(bool minimised, int window) {
   asm volatile (
      "int $0x30"
      :: "a" (24),
      "b" (minimised),
      "c" (window)
      : "cc", "memory"
   );
}

typedef struct font_info_t {
   int width;
   int height;
   int padding;
} font_info_t;

static inline font_info_t get_font_info() {
   font_info_t info;
      asm volatile (
      "int $0x30"
      : "=b" (info.width), "=c" (info.height), "=d" (info.padding)
      : "a" (61)
      : "cc", "memory"
   );
   return info;
}

static inline int create_window(int width, int height) {
   int index;
      asm volatile (
      "int $0x30"
      : "=b" (index)
      : "a" (62),
      "b" (width),
      "c" (height)
      : "cc", "memory"
   );
   return index;
}

static inline bool close_window(int index) {
   int success;
   asm volatile (
      "int $0x30"
      : "=b" (success)
      : "a" (63),
      "b" (index)
      : "cc", "memory"
   );
   return (bool)success;
}

static inline int create_thread(void (*func)()) {
   int id;
   asm volatile(
      "int $0x30"
      : "=b" (id)
      : "a" (65),
      "b" ((uint32_t)func)
      : "cc", "memory"
   );
   return id;
}

static inline tasks_t get_tasks() {
   tasks_t t;
   int addr;
   asm volatile (
      "int $0x30;"
      : "=b" (addr),
      "=c" (t.size)
      : "a" (66)
      : "cc", "memory"
   );
   t.tasks = (api_task_t*)addr;
   return t;
}

// terminal override

static inline void override_term_checkcmd(uint32_t addr) {
   asm volatile (
      "int $0x30;"
      :: "a" (39),
      "b" ((uint32_t)addr)
      : "cc", "memory"
   );
}

#endif