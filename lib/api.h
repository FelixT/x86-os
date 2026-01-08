// common c header between usrmode and kernel api

#include "stdint.h"
#include "stdbool.h"

typedef struct api_task_t {
   int id;
   bool enabled;
   bool paused;
   int parentid;
   // for processes
   uint32_t heap_start; // heap/end of ds (vmem location)
   uint32_t heap_end;
   uint32_t prog_start; // physical addr of start of program
   uint32_t prog_entry; // addr of program entry point, may be different from prog_start
   uint32_t prog_size;
   uint32_t vmem_start; // virtual address where program is loaded
   uint32_t vmem_end;
   int no_allocated;
   bool privileged;
   char working_dir[256];
   char main_window_name[32];
} api_task_t;

typedef struct tasks_t {
   api_task_t *tasks;
   int size; 
} tasks_t;

typedef enum {
   SETTING_WIN_BGCOLOUR,
   SETTING_WIN_TXTCOLOUR,
   SETTING_DESKTOP_ENABLED,
   SETTING_DESKTOP_BGIMG_PATH,
   SETTING_WIN_TITLEBARCOLOUR,
   SETTING_THEME_TYPE,
   SETTING_WIN_TITLEBARCOLOUR2,
   SETTING_SYS_FONT_PATH,
   SETTING_BGCOLOUR,
   SETTINGS_SYS_FONT_PADDING,
   SETTING_THEME_GRADIENTSTYLE
} api_setting_t;

#define W_SETTING_BGCOLOUR 0
#define W_SETTING_TXTCOLOUR 1
