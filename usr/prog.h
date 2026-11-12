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
      :: "a" (API_WRITE_STRING),
      "b" ((uint32_t)str),
      "c" (window)
      : "cc", "memory"
   );
}

static inline void write_str(char *str) {
   write_str_w(str, -1);
}

static inline void yield() {
   asm volatile(
      "int $0x30"
      :: "a" (API_YIELD)
      : "cc", "memory"
   );
}

static inline void write_uint(uint32_t num) {
   asm volatile(
      "int $0x30"
      :: "a" (API_WRITE_UINT),
      "b" (num)
      : "cc", "memory"
   );
}

static inline surface_t get_surface_w(int window) {
   surface_t surface;
   asm volatile (
      "int $0x30"
      : "=b" (surface.buffer), "=c" (surface.width), "=d" (surface.height)
      : "a" (API_RETURN_FRAMEBUFFER),
      "b" (window)
      : "cc", "memory"
   );
   surface.pitch = surface.width; // window buffers are always tightly packed
   return surface;
}

static inline surface_t get_surface() {
   return get_surface_w(-1);
}

static inline void redraw_w(int w) {
   asm volatile(
      "int $0x30"
      :: "a" (API_REDRAW_WINDOW),
      "b" (w)
      : "cc", "memory"
   );
}

static inline void redraw() {
   redraw_w(-1);
}

static inline void redraw_region(int x, int y, int width, int height, int window) {
   asm volatile(
      "int $0x30"
      :: "a" (API_REDRAW_REGION),
      "b" ((uint32_t)x),
      "c" ((uint32_t)y),
      "d" ((uint32_t)width),
      "S" ((uint32_t)height),
      "D" ((uint32_t)window)
      : "cc", "memory"
   );
}

// exit syscall
static inline __attribute__((noreturn)) void _exit(int status) {
   asm volatile(
      "int $0x30"
      :: "a" (API_END_TASK),
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
      :: "a" (API_END_SUBROUTINE)
      : "cc", "memory"
   );
}

static inline uint32_t get_task_event_count() {
   uint32_t event_count;
   asm volatile(
      "int $0x30"
      : "=b" (event_count)
      : "a" (API_GET_TASK_EVENT_COUNT)
      : "cc", "memory"
   );
   return event_count;
}

static inline bool wm_override(wm_action_t action, void *callback, int window) {
   bool success;
   asm volatile(
      "int $0x30"
      : "=b" (success)
      : "a" (API_WM_OVERRIDE),
      "b" (action),
      "c" ((uint32_t)callback),
      "d" (window)
      : "cc", "memory"
   );
   return success;
}

static inline int get_width_w(int window) {
   uint32_t output;

   asm volatile (
      "int $0x30;"
      : "=b" (output)
      : "a" (API_RETURN_WINDOW_WIDTH),
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
      : "a" (API_RETURN_WINDOW_HEIGHT),
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
      : "a" (API_MKDIR),
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
      : "a" (API_UNLINK),
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
      : "a" (API_RMDIR),
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
      : "a" (API_SEEK),
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
      :: "a" (API_DRAW_BMP),
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
      :: "a" (API_WRITE_STRING_AT),
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
      :: "a" (API_CLEAR_WINDOW),
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
      : "a" (API_READ_DIR),
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
      :: "a" (API_DEBUG_WRITE_STR),
      "b" (str)
      : "cc", "memory"
   );
}

static inline void queue_event(void *callback, int delta, void *msg) {
   asm volatile(
      "int $0x30"
      :: "a" (API_QUEUE_EVENT),
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
      : "a" (API_LAUNCH_TASK),
      "b" ((uint32_t)path),
      "c" ((uint32_t)argc),
      "d" ((uint32_t)args),
      "S" ((uint32_t)copy), // inherit fds and wd from current task
      "D" ((uint32_t)paused) // start paused
      : "cc", "memory"
   );
   return id;
}

static inline bool unpause_task(int id) {
   bool success;
   asm volatile (
      "int $0x30;"
      : "=b" (success)
      : "a" (API_UNPAUSE),
      "b" ((uint32_t)id)
      : "cc", "memory"
   );
   return success;
}

static inline void end_task(int id) {
   asm volatile (
      "int $0x30;"
      :: "a" (API_KILL_TASK),
      "b" ((uint32_t)id)
      : "cc", "memory"
   );
}

static inline bool set_setting(api_setting_t setting, uint32_t value) {
   int out;
   asm volatile (
      "int $0x30;"
      : "=b" (out)
      : "a" (API_SET_SETTING),
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
      : "a" (API_GET_SETTING),
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
      : "a" (API_SET_WINDOW_SETTING),
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
      : "a" (API_GET_WINDOW_SETTING),
      "b" ((uint32_t)setting),
      "c" ((uint32_t)window)
      : "cc", "memory"
   );
   return value;
}

static inline void set_window_title(char *title) {
   asm volatile (
      "int $0x30;"
      :: "a" (API_SET_WINDOW_TITLE),
      "b" ((uint32_t)title),
      "c" ((uint32_t)-1)
      : "cc", "memory"
   );
}

static inline void set_window_title_w(int window, char *title) {
   asm volatile (
      "int $0x30;"
      :: "a" (API_SET_WINDOW_TITLE),
      "b" ((uint32_t)title),
      "c" ((uint32_t)window)
      : "cc", "memory"
   );
}

static inline void chdir(char *path) {
   asm volatile (
      "int $0x30;"
      :: "a" (API_SET_WORKING_DIR),
      "b" ((uint32_t)path)
      : "cc", "memory"
   );
}

static inline void getwd(char *buf) {
   asm volatile (
      "int $0x30;"
      :: "a" (API_GET_WORKING_DIR),
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
      : "a" (API_OPEN),
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
      : "a" (API_FSIZE),
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
      : "a" (API_FPSIZE),
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
      : "a" (API_TRUNCATE),
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
      : "a" (API_READ),
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
      : "a" (API_WRITE),
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
      : "a" (API_NEW_FILE),
      "b" ((uint32_t)path)
      : "cc", "memory"
   );
   return fd;
}

static inline int close(int fd) {
   int r;
   asm volatile(
      "int $0x30"
      : "=b" (r)
      : "a" (API_CLOSE),
      "b" (fd)
      : "cc", "memory"
   );
   return r;
}

static inline bool rename(char *path, char *newname) {
   int success;
   asm volatile (
      "int $0x30"
      : "=b" (success)
      : "a" (API_RENAME),
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
      : "a" (API_SBRK),
      "b" ((uint32_t)increment)
      : "cc", "memory"
   );

   return (void*)addr;
}

static inline void create_scrollbar(void (*callback)(int deltaY, int offsetY, int window), int window) {
   asm volatile (
      "int $0x30"
      :: "a" (API_CREATE_SCROLLBAR),
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
      : "a" (API_SET_SCROLLABLE_HEIGHT),
      "b" (height),
      "c" (window)
      : "cc", "memory"
   );

   return width;
}

static inline void scroll_to(uint32_t y, int window) {
   asm volatile (
      "int $0x30"
      :: "a" (API_SCROLL_TO),
      "b" (y),
      "c" (window)
      : "cc", "memory"
   );
}

static inline void set_window_size(int width, int height, int w) {
   asm volatile (
      "int $0x30"
      :: "a" (API_SET_WINDOW_SIZE),
      "b" (width),
      "c" (height),
      "d" (w)
      : "cc", "memory"
   );
}

static inline void focus_task(int taskid) {
   asm volatile (
      "int $0x30"
      :: "a" (API_FOCUS_TASK),
      "b" (taskid)
      : "cc", "memory"
   );
}

static inline int get_taskid() {
   int taskid;
   asm volatile (
      "int $0x30"
      : "=b" (taskid)
      : "a" (API_GET_TASKID)
      : "cc", "memory"
   );
   return taskid;
}

static inline void set_window_position(int x, int y, int window) {
   asm volatile (
      "int $0x30"
      :: "a" (API_SET_WINDOW_POSITION),
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
      : "a" (API_GET_WINDOW_POSITION),
      "b" (window)
      : "cc", "memory"
   );
   coord_t coord = {x,y};
   return coord;
}

static inline void set_window_minimised(bool minimised, int window) {
   asm volatile (
      "int $0x30"
      :: "a" (API_SET_WINDOW_MINIMISED),
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
      : "a" (API_GET_FONT_INFO)
      : "cc", "memory"
   );
   return info;
}

static inline int create_window(int width, int height) {
   int index;
      asm volatile (
      "int $0x30"
      : "=b" (index)
      : "a" (API_CREATE_WINDOW),
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
      : "a" (API_CLOSE_WINDOW),
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
      : "a" (API_CREATE_THREAD),
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
      : "a" (API_GET_TASKS),
      "b" ((uint32_t)tasks),
      "c" (count)
      : "cc", "memory"
   );
   return size;
}

static inline void sleep(uint32_t ms) {
   asm volatile(
      "int $0x30"
      :: "a" (API_SLEEP),
      "b" (ms)
      : "cc", "memory"
   );
}

static inline uint32_t get_tick() {
   uint32_t ms;
   asm volatile(
      "int $0x30"
      : "=b" (ms)
      : "a" (API_GET_TIMER_TICK)
   );
   return ms;
}

static inline bool pipe(int *readfd, int *writefd) {
   bool success;
   int rfd, wfd;
   asm volatile(
      "int $0x30"
      : "=b" (success),
      "=c" (rfd),
      "=d" (wfd)
      : "a" (API_PIPE)
      : "cc", "memory"
   );
   *readfd = rfd;
   *writefd = wfd;
   return success;
}

static inline handle_t hdup(handle_t h, uint32_t rights) {
   handle_t out;
   asm volatile(
      "int $0x30"
      : "=b" (out)
      : "a" (API_DUP_HANDLE), "b" (h), "c" (rights)
      : "cc", "memory"
   );
   return out;
}

static inline int dup(int fd) {
   return hdup(fd, KOBJ_RIGHT_SAME);
}

static inline bool hinfo(handle_t h, kobj_type_t *type, uint32_t *rights) {
   int t;
   uint32_t r;
   asm volatile(
      "int $0x30"
      : "=b" (t), "=c" (r)
      : "a" (API_HANDLE_INFO), "b" (h)
      : "cc", "memory"
   );
   if(t < 0)
      return false;
   *type = t;
   *rights = r;
   return true;
}

static inline void dup2(int oldfd, int newfd) {
   asm volatile(
      "int $0x30"
      :: "a" (API_DUP2),
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
      : "a" (API_GET_TIME)
      : "cc", "memory"
   );
   return time;
}

static inline int futex_wait(void *addr, uint32_t expected) {
   int result;
   asm volatile(
      "int $0x30"
      : "=b" (result)
      : "a" (API_FUTEX_WAIT),
      "b" ((uint32_t)addr),
      "c" (expected)
      : "cc", "memory"
   );
   return result;
}

static inline void futex_wake(void *addr) {
   asm volatile(
      "int $0x30"
      :: "a" (API_FUTEX_WAKE),
      "b" ((uint32_t)addr)
      : "cc", "memory"
   );
}


static inline handle_t shared_create(int size) {
   handle_t h;
   asm volatile(
      "int $0x30"
      : "=b" (h)
      : "a" (API_SHARED_CREATE),
      "b" ((uint32_t)size)
      : "cc", "memory"
   );
   return h;
}

static inline void *map(handle_t handle, int index) {
   uint32_t addr;
   asm volatile(
      "int $0x30"
      : "=b" (addr)
      : "a" (API_MAP),
      "b" ((uint32_t)handle),
      "c" (index)
      : "cc", "memory"
   );
   return (void*)addr;
}

static inline bool unmap(uint32_t addr) {
   bool success;
   asm volatile(
      "int $0x30"
      : "=b" (success)
      : "a" (API_UNMAP),
      "b" (addr)
      : "cc", "memory"
   );
   return success;
}

static inline bool hcontrol(handle_t h, int operation, uint32_t arg, uint32_t *out) {
   bool success;
   uint32_t out_val;
   asm volatile(
      "int $0x30"
      : "=b" (success),
      "=c" (out_val)
      : "a" (API_CONTROL_HANDLE),
      "b" (h),
      "c" (operation),
      "d" (arg)
      : "cc", "memory"
   );

   if(out)
      *out = out_val;
   return success;
}

static inline handle_t pci_connect(uint16_t vendor, uint16_t device_id) {
   int h;
   asm volatile(
      "int $0x30"
      : "=b" (h)
      : "a" (API_PCI_CONNECT),
      "b" ((uint32_t)vendor),
      "c" ((uint32_t)device_id)
      : "cc", "memory"
   );
   return h;
}

static inline bool pci_exists(uint16_t vendor, uint16_t device_id) {
   bool exists;
   asm volatile(
      "int $0x30"
      : "=b" (exists)
      : "a" (API_PCI_EXISTS),
      "b" ((uint32_t)vendor),
      "c" ((uint32_t)device_id)
      : "cc", "memory"
   );
   return exists;
}

static inline handle_t dma(uint32_t size, uint32_t *paddr) {
   handle_t h;
   uint32_t p;

   asm volatile (
      "int $0x30"
      : "=b" (h),
      "=c" (p)
      : "a" (API_DMA),
      "b" (size)
      : "cc", "memory"
   );

   *paddr = p;
   return h;
}

// generic handle release
static inline uint32_t hrelease(handle_t handle) {
   bool success;
   asm volatile (
      "int $0x30;"
      : "=b" (success)
      : "a" (API_CLOSE_HANDLE),
      "b" (handle)
      : "cc", "memory"
   );
   return success;
}

static inline bool escalate() {
   bool success;
   asm volatile(
      "int $0x30"
      : "=b" (success)
      : "a" (API_ESCALATE)
      : "cc", "memory"
   );
   return success;
}

static inline int create_port(char *name, bool client_reserves, handle_t *port_h) {
   int status;
   handle_t port;
   asm volatile(
      "int $0x30"
      : "=b" (status), "=c" (port)
      : "a" (API_CREATE_PORT),
      "b" ((uint32_t)name),
      "c" (client_reserves)
      : "cc", "memory"
   );
   *port_h = port;
   return status;
}

static inline int port_connect(char *name, handle_t *conn_h) {
   int status;
   handle_t channel;
   asm volatile(
      "int $0x30"
      : "=b" (status),
      "=c" (channel)
      : "a" (API_PORT_CONNECT),
      "b" ((uint32_t)name)
      : "cc", "memory"
   );
   *conn_h = channel;
   return status;
}

static inline void override_msg(void (*msg_func)(handle_t conn_h, uint32_t flags)) {
   asm volatile(
      "int $0x30"
      :: "a" (API_OVERRIDE_MSG),
      "b" ((uint32_t)msg_func)
      : "cc", "memory"
   );
}

// wrap handle in msg_handle_t with default rights/flags
#define MSG_HANDLE(h) ((msg_handle_t){ (h), KOBJ_RIGHT_SAME, 0 })

// send msg_t obj (currently doesn't support handles)
static inline int msg_send_obj(handle_t conn_h, msg_send_t *msg, uint32_t flags) {
   int status;
   asm volatile(
      "int $0x30"
      : "=b" (status)
      : "a" (API_MSG_SEND),
      "b" (conn_h),
      "c" ((uint32_t)msg),
      "d" (flags)
      : "cc", "memory"
   );
   return status;
}

static inline int msg_send_flags(handle_t conn_h, void *buffer, int length, uint32_t flags) {
   msg_send_t msg = {.buffer = buffer, .size = length, .handle_count = 0};
   return msg_send_obj(conn_h, &msg, flags);
}

static inline int msg_send(handle_t conn_h, void *buffer, int length) {
   return msg_send_flags(conn_h, buffer, length, 0);
}

// send a request the receiver is expected to reply to. client_reserves reserved a slot in the reply queue
static inline int msg_request(handle_t conn_h, void *buffer, int length) {
   return msg_send_flags(conn_h, buffer, length, MSG_EXPECT_REPLY);
}

// answer MSG_EXPECT_REPLY (frees a reserved slot in senders queue)
static inline int msg_reply(handle_t conn_h, void *buffer, int length) {
   return msg_send_flags(conn_h, buffer, length, MSG_REPLY);
}

static inline int msg_read(handle_t conn_h, void *buffer, int size, uint32_t *channel_flags, uint32_t *msg_flags) {
   int status;
   uint32_t channel_flags_val;
   uint32_t msg_flags_val;
   asm volatile(
      "int $0x30"
      : "=b" (status),
      "=c" (channel_flags_val),
      "=d" (msg_flags_val)
      : "a" (API_MSG_READ),
      "b" (conn_h),
      "c" ((uint32_t)buffer),
      "d" (size)
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

static inline int msg_wait_for_read(handle_t conn_h) {
   int status;
   asm volatile(
      "int $0x30"
      : "=b" (status)
      : "a" (API_WAIT_ON_RECEIVE),
      "b" (conn_h)
      : "cc", "memory"
   );
   return status;
}

static inline bool snooze() {
   bool success;
   asm volatile(
      "int $0x30"
      : "=b" (success)
      : "a" (API_SNOOZE)
      : "cc", "memory"
   );
   return success;
}

// updates recv_msg
static inline int msg_sync_send_obj(handle_t conn_h, msg_send_t *send_msg, msg_recv_t *recv_msg) {
   int read_size;
   asm volatile(
      "int $0x30;"
      : "=b" (read_size)
      : "a" (API_MSG_CALL),
      "b" (conn_h),
      "c" ((uint32_t)send_msg),
      "d" ((uint32_t)recv_msg)
      : "cc", "memory"
   );
   return read_size;
}

static inline int msg_sync_send(handle_t conn_h, uint8_t *send_buffer, int send_size, uint8_t *receive_buffer, int receive_size) {
   msg_send_t send_msg = {.buffer = send_buffer, .size = send_size, .handle_count = 0};
   msg_recv_t recv_msg = {.buffer = receive_buffer, .buffer_size = receive_size, .handle_count = 0};
   return msg_sync_send_obj(conn_h, &send_msg, &recv_msg);
}

// updates recv_msg
static inline int msg_sync_receive_obj(handle_t port_h, msg_recv_t *recv_msg) {
   int bytes_read;
   asm volatile(
      "int $0x30;"
      : "=b" (bytes_read)
      : "a" (API_MSG_RECEIVE),
      "b" (port_h),
      "c" ((uint32_t)recv_msg)
      : "cc", "memory"
   );
   return bytes_read;
}

static inline int msg_sync_receive(handle_t port_h, uint8_t *receive_buffer, int receive_size, uint32_t *call_uid, handle_t *conn_h, uint32_t *flags) {
   msg_recv_t recv_msg = {.buffer = receive_buffer, .buffer_size = receive_size, .handle_count = 0};
   int r = msg_sync_receive_obj(port_h, &recv_msg);
   if(r >= 0) {
      *conn_h = recv_msg.conn_h;
      *call_uid = recv_msg.call_uid;
      *flags = recv_msg.flags;
   }
   return r;
}

static inline int msg_sync_reply_obj(handle_t conn_h, uint32_t call_uid, msg_send_t *send_msg) {
   int sent_bytes;
   asm volatile(
      "int $0x30;"
      : "=b" (sent_bytes)
      : "a" (API_MSG_REPLY),
      "b" (conn_h),
      "c" (call_uid),
      "d" ((uint32_t)send_msg)
      : "cc", "memory"
   );
   return sent_bytes;
}

static inline int msg_sync_reply(handle_t conn_h, uint32_t call_uid, uint8_t *send_buffer, int send_size) {
   msg_send_t send_msg = {.buffer = send_buffer, .size = send_size, .handle_count = 0};
   return msg_sync_reply_obj(conn_h, call_uid, &send_msg);
}

#endif