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

// exit syscall
static inline __attribute__((noreturn)) void _exit(int status) {
   asm volatile(
      "int $0x30"
      :: "a" (10),
      "b" (status)
      : "cc", "memory"
   );
   __builtin_unreachable();
}

extern void fclose_all(void) __attribute__((weak));

static inline __attribute__((noreturn)) void exit(int status) {
   if(fclose_all) // if linked with stdio
      fclose_all();
   _exit(status);
}

static inline void end_subroutine() {
   asm volatile(
      "int $0x30"
      :: "a" (12)
      : "cc", "memory"
   );
}

static inline void override_click(void *callback, int window) {
   asm volatile(
      "int $0x30"
      :: "a" (13),
      "b" ((uint32_t)callback),
      "c" (window)
      : "cc", "memory"
   );
}

static inline void override_draw(void *callback, int window) {
   asm volatile(
      "int $0x30"
      :: "a" (29),
      "b" ((uint32_t)callback),
      "c" (window)
      : "cc", "memory"
   );
}

static inline int get_width_w(int window) {
   uint32_t output;

   asm volatile (
      "int $0x30;"
      : "=b" (output)
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
      "int $0x30"
      : "=b" (output)
      : "a" (15),
      "b" (window)
      : "cc", "memory"
   );

   return output;
}

static inline int get_height() {
   return get_height_w(-1);
}

static inline bool mkdir(char *path) {
   int success;
   asm volatile (
      "int $0x30"
      : "=b" (success)
      : "a" (50),
      "b" ((uint32_t)path)
      : "cc", "memory"
   );
   return (bool)success;
}

static inline bool unlink(char *path) {
   int success;
   asm volatile (
      "int $0x30"
      : "=b" (success)
      : "a" (68),
      "b" ((uint32_t)path)
      : "cc", "memory"
   );
   return (bool)success;
}

static inline bool rmdir(char *path) {
   int success;
   asm volatile (
      "int $0x30"
      : "=b" (success)
      : "a" (69),
      "b" ((uint32_t)path)
      : "cc", "memory"
   );
   return (bool)success;
}

static inline int seek(int fd, int offset, int type) {
   int pos;
   asm volatile (
      "int $0x30"
      : "=b" (pos)
      : "a" (48),
      "b" (fd),
      "c" (offset),
      "d" (type)
      : "cc", "memory"
   );
   return pos;
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

// count = max entries to read (0 allowed)
// returns -1 on fail, dir size otherwise
static inline int read_dir(char *path, fs_dir_entry_t *entries, int count) {
   int result;

   asm volatile (
      "int $0x30"
      : "=b" (result)
      : "a" (25),
      "b" ((uint32_t)path),
      "c" (count),
      "d" ((uint32_t)entries)
      : "cc", "memory"
   );

   return result;
}

static inline void debug_write_str(char *str) {
   asm volatile(
      "int $0x30"
      :: "a" (49),
      "b" (str)
      : "cc", "memory"
   );
}

static inline void override_resize(void *callback, int window) {
   asm volatile(
      "int $0x30"
      :: "a" (34),
      "b" ((uint32_t)callback),
      "c" (window)
      : "cc", "memory"
   );
}

static inline void override_drag(void *callback, int window) {
   asm volatile(
      "int $0x30"
      :: "a" (36),
      "b" ((uint32_t)callback),
      "c" (window)
      : "cc", "memory"
   );
}

static inline void override_release(void *callback, int window) {
   asm volatile(
      "int $0x30"
      :: "a" (38),
      "b" ((uint32_t)callback),
      "c" (window)
      : "cc", "memory"
   );
}

static inline void override_keypress(void *callback, int window) {
   asm volatile(
      "int $0x30"
      :: "a" (64),
      "b" ((uint32_t)callback),
      "c" (window)
      : "cc", "memory"
   );
}

static inline void override_keyrelease(void *callback, int window) {
   asm volatile(
      "int $0x30"
      :: "a" (46),
      "b" ((uint32_t)callback),
      "c" (window)
      : "cc", "memory"
   );
}

static inline bool override_close(void *callback, int window) {
   bool success;
   asm volatile(
      "int $0x30"
      : "=b" (success)
      : "a" (27),
      "b" ((uint32_t)callback),
      "c" (window)
      : "cc", "memory"
   );
   return success;
}

static inline void override_rightclick(void *callback, int window) {
   asm volatile(
      "int $0x30"
      :: "a" (17),
      "b" ((uint32_t)callback),
      "c" (window)
      : "cc", "memory"
   );
}

static inline void override_hover(void *callback, int window) {
   asm volatile(
      "int $0x30"
      :: "a" (11),
      "b" ((uint32_t)callback),
      "c" (window)
      : "cc", "memory"
   );
}

static inline void override_mouseout(void *callback, int window) {
   asm volatile(
      "int $0x30"
      :: "a" (45),
      "b" ((uint32_t)callback),
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

static inline void queue_event(void *callback, int delta, void *msg) {
   asm volatile(
      "int $0x30"
      :: "a" (30),
      "b" ((uint32_t)callback),
      "c" ((uint32_t)delta),
      "d" ((uint32_t)msg)
      : "cc", "memory"
   );
}

static inline int launch_task(char *path, int argc, char **args, bool copy, bool paused) {
   // copy args to kernel memory
   int id; 
   asm volatile (
      "int $0x30;"
      : "=b" (id)
      : "a" (32),
      "b" ((uint32_t)path),
      "c" ((uint32_t)argc),
      "d" ((uint32_t)args),
      "S" ((uint32_t)copy), // inherit fds and wd from current task
      "D" ((uint32_t)paused) // start paused
      : "cc", "memory"
   );
   return id;
}

static inline void unpause_task(int id) {
   asm volatile (
      "int $0x30;"
      :: "a" (71),
      "b" ((uint32_t)id)
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

// for reading strings, out buffer must be >=256 bytes
static inline uint32_t get_setting(api_setting_t setting, char *out) {
   uint32_t value;
   asm volatile (
      "int $0x30;"
      : "=b" (value)
      : "a" (33),
      "b" ((uint32_t)setting),
      "c" ((uint32_t)out)
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

#define FS_FLAG_WRITEONLY 1
#define FS_FLAG_READONLY 2
#define FS_FLAG_TRUNCATE 4
#define FS_FLAG_APPEND 8
#define FS_FLAG_CREATE 16

static inline int open(char *path, int flag) {
   int fd;
   asm volatile (
      "int $0x30"
      : "=b" (fd)
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
      "int $0x30"
      : "=b" (size)
      : "a" (59),
      "b" ((uint32_t)fd)
      : "cc", "memory"
   );
   return size;
}

static inline int fpsize(char *path) {
   int size;
   asm volatile (
      "int $0x30"
      : "=b" (size)
      : "a" (87),
      "b" ((uint32_t)path)
      : "cc", "memory"
   );
   return size;
}

static inline int ftruncate(int fd, int size) {
   int result;
   asm volatile (
      "int $0x30"
      : "=b" (result)
      : "a" (88),
      "b" ((uint32_t)fd),
      "c" ((uint32_t)size)
      : "cc", "memory"
   );
   return result;
}

static inline int read(int fd, char *buf, size_t count) {
   int c;
   asm volatile (
      "int $0x30"
      : "=b" (c)
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
      "int $0x30"
      : "=b" (c)
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
      "int $0x30"
      : "=b" (fd)
      : "a" (41),
      "b" ((uint32_t)path)
      : "cc", "memory"
   );
   return fd;
}

static inline int close(int fd) {
   asm volatile(
      "int $0x30"
      :: "a" (73),
      "b" (fd)
      : "cc", "memory"
   );
   return 0;
}

static inline bool rename(char *path, char *newname) {
   int success;
   asm volatile (
      "int $0x30"
      : "=b" (success)
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
      "int $0x30"
      : "=b" (addr)
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
      "int $0x30"
      : "=b" (width)
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

static inline int get_tasks(api_task_t *tasks, int count) {
   int size;
   asm volatile (
      "int $0x30;"
      : "=b" (size)
      : "a" (66),
      "b" ((uint32_t)tasks),
      "c" (count)
      : "cc", "memory"
   );
   return size;
}

static inline void sleep(uint32_t ms) {
   asm volatile(
      "int $0x30"
      :: "a" (31),
      "b" (ms)
      : "cc", "memory"
   );
}

static inline uint32_t get_tick() {
   uint32_t ms;
   asm volatile(
      "int $0x30"
      : "=b" (ms)
      : "a" (57)
   );
   return ms;
}

static inline void pipe(int *readfd, int *writefd) {
   int rfd, wfd;
   asm volatile(
      "int $0x30"
      : "=b" (rfd), "=c" (wfd)
      : "a" (70)
      : "cc", "memory"
   );
   *readfd = rfd;
   *writefd = wfd;
}

static inline int dup(int oldfd) {
   int new_fd;
   asm volatile(
      "int $0x30"
      : "=b" (new_fd)
      : "a" (74), "b" (oldfd)
      : "cc", "memory"
   );
   return new_fd;
}

static inline void dup2(int oldfd, int newfd) {
   asm volatile(
      "int $0x30"
      :: "a" (72),
      "b" (oldfd),
      "c" (newfd)
      : "cc", "memory"
   );
}

static inline uint32_t get_time() {
   uint32_t time;
   asm volatile(
      "int $0x30"
      : "=b" (time)
      : "a" (75)
      : "cc", "memory"
   );
   return time;
}

static inline int futex_wait(void *addr, uint32_t expected) {
   int result;
   asm volatile(
      "int $0x30"
      : "=b" (result)
      : "a" (76),
      "b" ((uint32_t)addr),
      "c" (expected)
      : "cc", "memory"
   );
   return result;
}

static inline void futex_wake(void *addr) {
   asm volatile(
      "int $0x30"
      :: "a" (77),
      "b" ((uint32_t)addr)
      : "cc", "memory"
   );
}

typedef struct shared_t {
   uint32_t uid;
   void *mem;
} shared_t;

static inline shared_t shared_create(int size) {
   shared_t shared;
   asm volatile(
      "int $0x30"
      : "=b" (shared.mem),
      "=c" (shared.uid)
      : "a" (78),
      "b" ((uint32_t)size)
      : "cc", "memory"
   );
   return shared;
}

static inline void shared_grant(int task_id, uint32_t block_uid) {
   asm volatile(
      "int $0x30"
      :: "a" (79),
      "b" ((uint32_t)task_id),
      "c" (block_uid)
      : "cc", "memory"
   );
}

static inline void *shared_map(uint32_t block_uid) {
   uint32_t addr;
   asm volatile(
      "int $0x30"
      : "=b" (addr)
      : "a" (80),
      "b" ((uint32_t)block_uid)
      : "cc", "memory"
   );
   return (void*)addr;
}

static inline bool shared_close(uint32_t block_uid) {
   bool success;
   asm volatile(
      "int $0x30"
      : "=b" (success)
      : "a" (81),
      "b" ((uint32_t)block_uid)
      : "cc", "memory"
   );
   return success;
}

static inline uint8_t *pci_map(uint16_t vendor, uint16_t device_id) {
   uint32_t addr;
   asm volatile(
      "int $0x30"
      : "=b" (addr)
      : "a" (82),
      "b" ((uint32_t)vendor),
      "c" ((uint32_t)device_id)
      : "cc", "memory"
   );
   return (uint8_t*)addr;
}

static inline bool pci_exists(uint16_t vendor, uint16_t device_id) {
   bool exists;
   asm volatile(
      "int $0x30"
      : "=b" (exists)
      : "a" (83),
      "b" ((uint32_t)vendor),
      "c" ((uint32_t)device_id)
      : "cc", "memory"
   );
   return exists;
}

static inline void *dma(uint32_t size) {
   uint32_t addr;

   asm volatile (
      "int $0x30"
      : "=b" (addr)
      : "a" (84),
      "b" (size)
      : "cc", "memory"
   );

   return (void*)addr;
}

static inline void dma_free(void *addr, uint32_t size) {
   asm volatile (
      "int $0x30;"
      :: "a" (85),
      "b" ((uint32_t)addr),
      "c" (size)
      : "cc", "memory"
   );
}

static inline bool escalate() {
   bool success;
   asm volatile(
      "int $0x30"
      : "=b" (success)
      : "a" (86)
      : "cc", "memory"
   );
   return success;
}

// returns a port uid, or a msg_err_t - test with MSG_IS_ERROR (lib/api.h)
static inline uint32_t create_port(char *name, bool client_reserves) {
   uint32_t port;
   asm volatile(
      "int $0x30"
      : "=b" (port)
      : "a" (89),
      "b" ((uint32_t)name),
      "c" (client_reserves)
      : "cc", "memory"
   );
   return port;
}

// returns a channel uid, or a msg_err_t - test with MSG_IS_ERROR (lib/api.h)
static inline uint32_t port_connect(char *name, uint32_t *port) {
   uint32_t port_uid;
   uint32_t channel;
   asm volatile(
      "int $0x30"
      : "=b" (channel),
      "=c" (port_uid)
      : "a" (90),
      "b" ((uint32_t)name)
      : "cc", "memory"
   );
   *port = port_uid;
   return channel;
}

static inline void override_msg(void (*msg_func)(uint32_t port_uid, uint32_t channel_uid, uint32_t flags)) {
   asm volatile(
      "int $0x30"
      :: "a" (91),
      "b" ((uint32_t)msg_func)
      : "cc", "memory"
   );
}

static inline int msg_send_flags(uint32_t port_uid, uint32_t channel_uid, void *buffer, int length, uint32_t flags) {
   int status;
   asm volatile(
      "int $0x30"
      : "=b" (status)
      : "a" (92),
      "b" (port_uid),
      "c" (channel_uid),
      "d" ((uint32_t)buffer),
      "S" (length),
      "D" (flags)
      : "cc", "memory"
   );
   return status;
}

static inline int msg_send(uint32_t port_uid, uint32_t channel_uid, void *buffer, int length) {
   return msg_send_flags(port_uid, channel_uid, buffer, length, 0);
}

// send a request the receiver is expected to reply to. client_reserves reserved a slot in the reply queue
static inline int msg_request(uint32_t port_uid, uint32_t channel_uid, void *buffer, int length) {
   return msg_send_flags(port_uid, channel_uid, buffer, length, MSG_EXPECT_REPLY);
}

// answer MSG_EXPECT_REPLY (frees a reserved slot in senders queue)
static inline int msg_reply(uint32_t port_uid, uint32_t channel_uid, void *buffer, int length) {
   return msg_send_flags(port_uid, channel_uid, buffer, length, MSG_REPLY);
}

static inline int msg_read(uint32_t port_uid, uint32_t channel_uid, void *buffer, int size, uint32_t *channel_flags, uint32_t *msg_flags) {
   int status;
   uint32_t channel_flags_val;
   uint32_t msg_flags_val;
   asm volatile(
      "int $0x30"
      : "=b" (status),
      "=c" (channel_flags_val),
      "=d" (msg_flags_val)
      : "a" (93),
      "b" (port_uid),
      "c" (channel_uid),
      "d" ((uint32_t)buffer),
      "S" (size)
      : "cc", "memory"
   );
   if(status < 0) {
      channel_flags_val = 0;
      msg_flags_val = 0;
   }
   *channel_flags = channel_flags_val;
   *msg_flags = msg_flags_val;
   return status;
}

static inline bool msg_wait_for_read(uint32_t port_uid, uint32_t channel_uid) {
   bool success;
   asm volatile(
      "int $0x30"
      : "=b" (success)
      : "a" (94),
      "b" (port_uid),
      "c" (channel_uid)
      : "cc", "memory"
   );
   return success;
}

static inline bool snooze() {
   bool success;
   asm volatile(
      "int $0x30"
      : "=b" (success)
      : "a" (95)
      : "cc", "memory"
   );
   return success;
}

static inline bool port_close(uint32_t port_uid) {
   bool success;
   asm volatile(
      "int $0x30"
      : "=b" (success)
      : "a" (96),
      "b" (port_uid)
      : "cc", "memory"
   );
   return success;
}

static inline bool port_disconnect(uint32_t port_uid, uint32_t channel_uid) {
   bool success;
   asm volatile(
      "int $0x30"
      : "=b" (success)
      : "a" (97),
      "b" (port_uid),
      "c" (channel_uid)
      : "cc", "memory"
   );
   return success;
}

static inline int msg_sync_send(uint32_t channel_uid, uint8_t *send_buffer, int send_size, uint8_t *receive_buffer, int receive_size) {
   int read_size;
   int sent_size;
   asm volatile(
      "int $0x30;"
      : "=b" (read_size),
      "=c" (sent_size)
      : "a" (98),
      "b" (channel_uid),
      "c" ((uint32_t)send_buffer),
      "d" (send_size),
      "S" ((uint32_t)receive_buffer),
      "D" (receive_size)
      : "cc", "memory"
   );
   return read_size;
}

static inline int msg_sync_receive(uint32_t port_uid, uint8_t *receive_buffer, int receive_size, uint32_t *call_id) {
   int read_size;
   int sent_size;
   uint32_t call_id_in;
   asm volatile(
      "int $0x30;"
      : "=b" (read_size),
      "=c" (sent_size),
      "=d" (call_id_in)
      : "a" (99),
      "b" (port_uid),
      "c" ((uint32_t)receive_buffer),
      "d" (receive_size)
      : "cc", "memory"
   );
   *call_id = call_id_in;
   return read_size;
}

static inline int msg_sync_reply(uint32_t call_uid, uint8_t *send_buffer, int send_size) {
   int sent;
   asm volatile(
      "int $0x30;"
      : "=b" (sent)
      : "a" (100),
      "b" (call_uid),
      "c" ((uint32_t)send_buffer),
      "d" (send_size)
      : "cc", "memory"
   );
   return sent;
}

// terminal override

static inline void override_term_checkcmd(void (*callback)(char *cmd)) {
   asm volatile (
      "int $0x30;"
      :: "a" (39),
      "b" ((uint32_t)callback)
      : "cc", "memory"
   );
}

#endif