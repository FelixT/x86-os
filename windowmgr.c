#include "windowmgr.h"
#include "memory.h"
#include "window.h"
#include "tasks.h"
#include "draw.h"
#include "lib/string.h"
#include "lib/maths.h"
#include "bmp.h"
#include "windowobj.h"
#include "events.h"

#include "window_term.h"
#include "window_settings.h"
#include "window_popup.h"
#include <stdarg.h>

int windowCount = 0;
int gui_selected_window = -1;
gui_window_t *selectedWindow = NULL;

gui_window_t *gui_windows;

extern surface_t surface; // screen
extern uint16_t *draw_buffer;
extern surface_t main_surface; // screen, even while surface is swapped to draw_buffer

extern bool gui_cursor_shown;
extern int gui_mouse_x;
extern int gui_mouse_y;
extern int cursor_oldx;
extern int cursor_oldy;
extern bool mouse_heldright;
extern uint16_t gui_bg;
extern uint16_t *draw_buffer;

static int *outline_buffer;
static bool outline_saved = false;

static gui_window_t *dragged_window = NULL;
static gui_window_t *pressed_window = NULL; // window sent the last click, gets corresponding release

windowmgr_settings_t wm_settings = {
   .default_window_bgcolour = 0xFFBE, // 0xFFFF
   .default_window_txtcolour = 0x0000,
   .desktop_enabled = false,
   .desktop_bgimg_enabled = true,
   .titlebar_colour = COLOUR_TITLEBAR_CLASSIC,
   .titlebar_colour2 = COLOUR_TITLEBAR_COLOUR2,
   .titlebar_gradientstyle = 1, // vertical
   .theme = 0
};

uint8_t *icon_window = NULL;
uint8_t *gui_bgimage = NULL;
uint8_t *icon_files = NULL;
int gui_bgimage_size = 0;

windowobj_t *default_menu; // right click menu
windowobj_t *app_button; // toolbar app button

// window manager kernel threads
// thread for draws
// thread for disk reads
// thread for keyboard/mouse

// draw thread
typedef struct wm_thread_t {
   task_state_t *task; // kthread
   bool redrawall_pending;
   bool draw_pending; // redraw all current window
   int paused_task;
} wm_thread_t;

// task launch thread

typedef struct wm_launch_event_t {
   char path[256];
   bool elf; // false for binary
   char **args;
   int argc;
   bool focus;
   bool minimised;
} wm_launch_event_t;

#define WM_LAUNCH_QUEUE_SIZE 4

typedef struct wm_launch_thread_t {
   task_state_t *task; // kthread
   wm_launch_event_t queue[WM_LAUNCH_QUEUE_SIZE];
   int queue_head, queue_tail, queue_size;
} wm_launch_thread_t;

// event thread

#define WM_EVENT_QUEUE_SIZE 32

typedef struct wm_event_t {
   wm_event_type_t type;
   int x, y;
   uint16_t c;
} wm_event_t;

typedef struct wm_event_thread_t {
   task_state_t *task; // kthread
   wm_event_t queue[WM_EVENT_QUEUE_SIZE];
   int queue_head, queue_tail, queue_size;
   int paused_task;
   int yielding_to; // queued until end of event
} wm_event_thread_t;

static wm_thread_t wm_thread = {.task=NULL, .draw_pending = false, .redrawall_pending = false, .paused_task = -1};
static wm_launch_thread_t wm_launch_thread = {.task=NULL, .queue_head = 0, .queue_tail = 0, .queue_size = 0};
static wm_event_thread_t wm_event_thread = {.task=NULL, .queue_head = 0, .queue_tail = 0, .queue_size = 0, .paused_task = -1, .yielding_to = -1};

void windowmgr_draw();

// main function of windowmgr kthread
void wm_thread_main() {
   wm_thread.task = get_current_task_state();

   while(true) {
      if(wm_thread.redrawall_pending) {
         wm_thread.redrawall_pending = false;
         wm_thread.draw_pending = false;
         windowmgr_redrawall();
      }

      if(wm_thread.draw_pending) {
         wm_thread.draw_pending = false;
         windowmgr_draw();
      }

      task_pause(get_current_task_state(), PAUSE_KTHREAD);
      int paused_task = wm_thread.paused_task;
      wm_thread.paused_task = -1;
      if(paused_task < 0 || !kernel_yield_to(paused_task))
         kernel_yield();
   }
}

void wm_thread_wake() {
   if(!wm_thread.task || wm_thread.task->pause_reason != PAUSE_KTHREAD)
      return;
   wm_thread.paused_task = get_current_task();
   task_resume(wm_thread.task);
   kernel_yield_to(wm_thread.task->task_id);
}

void wm_draw() {
   wm_thread.draw_pending = true;
   wm_thread_wake();
}

void wm_redrawall() {
   wm_thread.redrawall_pending = true;
   wm_thread_wake();
}

void wm_launch_thread_main() {
   wm_launch_thread.task = get_current_task_state();

   while(true) {
      if(!wm_launch_thread.queue_size) {
         task_pause(get_current_task_state(), PAUSE_KTHREAD);
         kernel_yield();
         continue;
      }
      wm_launch_event_t *launch = &wm_launch_thread.queue[wm_launch_thread.queue_head];
      if(!launch->elf) {
         // binary
         tasks_launch_binary(NULL, launch->path);
      } else {
         // elf
         int new_task = tasks_setup_elf(launch->path, launch->argc, launch->args, launch->focus, false, launch->minimised);
         if(new_task < 0) {
            debug_printf("wm_launch %s failed\n", launch->path);
            free_launch_args(launch->args, launch->argc);
         } else {
            gettasks()[new_task].enabled = true;
            // map args
            if(launch->args) {
               page_dir_entry_t *dir = gettasks()[new_task].process->page_dir;
               map_size(dir, (uint32_t)launch->args, (uint32_t)launch->args, sizeof(char*)*(launch->argc+1), 1, 1, 0);
               for(int i = 0; i < launch->argc; i++)
                  map_size(dir, (uint32_t)launch->args[i], (uint32_t)launch->args[i], strlen(launch->args[i])+1, 1, 1, 0);
            }
         }
      }
      // remove from queue
      wm_launch_thread.queue_size--;
      wm_launch_thread.queue_head++;
      wm_launch_thread.queue_head%=WM_LAUNCH_QUEUE_SIZE;

      kernel_yield_if_blocking();
   }
}

void wm_queue_launch(char *path, bool elf, char **args, int argc, bool focus, bool minimised) {
   if(wm_launch_thread.queue_size == WM_LAUNCH_QUEUE_SIZE) {
      debug_printf("WM: launch %s failed, queue is full\n", path);
      free_launch_args(args, argc);
      return;
   }
   wm_launch_event_t *launch = &wm_launch_thread.queue[wm_launch_thread.queue_tail];
   strcpy(launch->path, path);
   launch->focus = focus;
   launch->minimised = minimised;
   launch->elf = elf;
   launch->args = args;
   launch->argc = argc;
   wm_launch_thread.queue_tail++;
   wm_launch_thread.queue_tail%=WM_LAUNCH_QUEUE_SIZE;
   wm_launch_thread.queue_size++;
   if(wm_launch_thread.task)
      task_resume(wm_launch_thread.task);
}

void windowmgr_release(int x, int y);

void wm_event_defer_yield(int task) {
   if(wm_event_thread.yielding_to == task)
      debug_printf("wm_event_defer_yield: yield to same task");
   if(wm_event_thread.yielding_to != -1)
      debug_printf("wm_event_defer_yield: overwrote %i\n", wm_event_thread.yielding_to);
   wm_event_thread.yielding_to = task;
}

// deferred yield to avoid yielding midway through wm code
void wm_call_subroutine(task_state_t *task, char *name, uint32_t addr, uint32_t *args, int argc) {
   if(!wm_event_thread.task || get_current_task() != wm_event_thread.task->task_id) {
      debug_printf("wm_call_subroutine called from outside wm event thread\n");
      kernel_panic();
   }
   task_queue_subroutine(task, name, addr, args, argc);
   wm_event_defer_yield(task->task_id);
}

void wm_event_thread_main() {
   wm_event_thread.task = get_current_task_state();

   while(true) {
      if(!wm_event_thread.queue_size) {
         int paused = wm_event_thread.paused_task;
         wm_event_thread.paused_task = -1;
         if(paused >= 0 && kernel_yield_to(paused))
            continue;
         
         task_pause(get_current_task_state(), PAUSE_KTHREAD);
         kernel_yield();
         continue;
      }
      wm_event_t event = wm_event_thread.queue[wm_event_thread.queue_head];
      // remove from queue
      wm_event_thread.queue_size--;
      wm_event_thread.queue_head++;
      wm_event_thread.queue_head%=WM_EVENT_QUEUE_SIZE;

      wm_event_thread.yielding_to = -1;
      switch(event.type) {
         case MOUSE_CLICK:
            if(!windowmgr_click(event.x, event.y))
               desktop_click(event.x, event.y);
            break;
         case MOUSE_RELEASE:
            windowmgr_release(event.x, event.y);
            break;
         case MOUSE_DRAG:
            windowmgr_dragged(event.x, event.y);
            break;
         case MOUSE_HOVER:
            windowmgr_mousemove(event.x, event.y);
            break;
         case MOUSE_RIGHTCLICK:
            windowmgr_rightclick(event.x, event.y);
            break;
         case MOUSE_SCROLL:
            windowmgr_scroll(event.x, event.y);
            break;
         case KEY_PRESS:
            windowmgr_keypress(event.c);
            break;
         case WINDOW_CLOSE:
            int w = event.x;
            int t = get_task_from_window(w);
            if(t != -1) break; // window already closed+reassigned
            window_close(w, true);
            gui_redrawall();
            break;
         case TASK_CRASH:
            int task_id = event.x;
            uint32_t process_uid = event.y;
            uint16_t int_no = event.c;
            show_endtask_dialog(task_id, process_uid, int_no);
            break;
         default:
            //
      }
      
      int yield = wm_event_thread.yielding_to;
      wm_event_thread.yielding_to = -1;
      if(yield < 0 || !kernel_yield_to(yield)) {
         int paused = wm_event_thread.paused_task;
         wm_event_thread.paused_task = -1;
         if(paused < 0 || !kernel_yield_to(paused))
            kernel_yield();
      }
   }
}

void wm_event_thread_wake() {
   if(!wm_event_thread.task || wm_event_thread.task->pause_reason != PAUSE_KTHREAD)
      return;
   wm_event_thread.paused_task = get_current_task();
   task_resume(wm_event_thread.task);
   kernel_yield_to(wm_event_thread.task->task_id);
}

// wm_events are queued from irqs so can't yield
// instead, immediately invoke the wm_event thread at the end of the handler
void wm_event_thread_exit_resume(registers_t *regs) {
   if(!wm_event_thread.task || wm_event_thread.queue_size == 0) return;
   kernel_exit_resume(regs, wm_event_thread.task->task_id);
}

void wm_event(wm_event_type_t type, int x, int y, uint16_t c) {
   if((type == MOUSE_HOVER || type == MOUSE_DRAG || type == MOUSE_SCROLL) && wm_event_thread.queue_size) {
      // coalesce into most recent event
      int idx = (wm_event_thread.queue_tail+WM_EVENT_QUEUE_SIZE-1)%WM_EVENT_QUEUE_SIZE;
      wm_event_t *e = &wm_event_thread.queue[idx];
      bool merged = true;
      if(e->type == type) {
         if(type == MOUSE_HOVER) {
            e->x = x;
            e->y = y;
         } else if(type == MOUSE_DRAG) {
            e->x += x; // relX,Y
            e->y += y;
         } else if(type == MOUSE_SCROLL && e->y == y) { // for same window
            if(y == -1)
               e->x += x; // deltaY
            else
               e->x = x; // scrolledY
         } else {
            merged = false;
         }
         if(merged) {
            wm_event_thread_wake();
            return;
         }
      }
   }
   if(wm_event_thread.queue_size == WM_EVENT_QUEUE_SIZE) {
      debug_printf("WM: event failed, queue is full\n"); // todo: crashes shouldn't be dropped
      return;
   }
   wm_event_t *event = &wm_event_thread.queue[wm_event_thread.queue_tail];
   event->type = type;
   event->x = x;
   event->y = y;
   event->c = c;
   wm_event_thread.queue_tail++;
   wm_event_thread.queue_tail%=WM_EVENT_QUEUE_SIZE;
   wm_event_thread.queue_size++;
   wm_event_thread_wake();
}

gui_window_t *render_order[MAX_WINDOWS];

void debug_writestr(char *str) {
   if(windowCount == 0) return;
   window_writestr(str, 0, 0);
}

void debug_writeuint(uint32_t num) {
   if(windowCount == 0) return;
   window_writeuint(num, 0, 0);
}

void debug_writehex(uint32_t num) {
   if(windowCount == 0) return;
   char out[20];
   uinttohexstr(num, out);
   window_writestr("0x", 0, 0);
   window_writestr(out, 0, 0);
}

void debug_printf(char *format, ...) {
   char buffer[512];
   va_list args;
   va_start(args, format);
   vsnprintf(buffer, 512, format, args);
   va_end(args);
   debug_writestr(buffer);
}

int getFirstFreeIndex() {
   for(int i = 0; i < windowCount; i++) {
      if(gui_windows[i].closed)
        return i;
   }

   return -1;
}

int getSelectedWindowIndex() {
   return gui_selected_window;
}

// move selected window to front of render order
void update_render_order() {
   if(selectedWindow == NULL) return;

   int found = -1;
   for(int i = 0; i < windowCount; i++) {
      if(render_order[i] == selectedWindow) {
         found = i;
         break;
      }
   }

   if(found == -1) {
      for(int i = windowCount - 1; i > 0; i--) {
         render_order[i] = render_order[i-1];
      }
      render_order[0] = selectedWindow;
   } else if(found > 0) {
      gui_window_t *temp = render_order[found];
      for(int i = found; i > 0; i--) {
         render_order[i] = render_order[i-1];
      }
      render_order[0] = temp;
   }
}

void setSelectedWindowIndex(int index) {
   if(selectedWindow != NULL)
      selectedWindow->active = false;
   gui_selected_window = index;
   if(index == -1) {
      selectedWindow = NULL;
   } else {
      selectedWindow = &gui_windows[index];
      selectedWindow->active = true;
   }

   update_render_order();
}

int getWindowCount() {
   return windowCount;
}

gui_window_t *getWindow(int index) {
   return &gui_windows[index];
}

gui_window_t *getSelectedWindow() {
   if(gui_selected_window == -1) return NULL;
   return &gui_windows[gui_selected_window];
}

void window_close(int windowIndex, bool end) {
   if(windowIndex < 0 || windowIndex >= getWindowCount()) return;
   gui_window_t *window = getWindow(windowIndex);
   if(window->closed) return;
   if(pressed_window == window) pressed_window = NULL;

   if(windowIndex == 0) {
      // debug window
      window_minimise(getWindow(windowIndex), true);
      int popup = windowmgr_add();
      if(popup < 0) {
         gui_redrawall();
         return;
      }
      window_popup_dialog(getWindow(popup), getWindow(windowIndex), "Minimised debug window");
      setSelectedWindowIndex(popup);
      gui_redrawall();
      return;
   }

   int task = get_task_from_window(windowIndex);
   task_state_t *task_state = task > -1 ? &gettasks()[task] : NULL;
   bool ending_task = end && task_state && task_state->process->window == windowIndex;
   task_state_t *read_task = window->read_task >= 0 ? &gettasks()[window->read_task] : NULL;

   // notify window read_task (unless it's about to be killed)
   if(read_task && read_task->enabled && (!ending_task || read_task->process != task_state->process))
      fs_read_window_callback(window, true);

   bool do_callback = false;
   int cindex = task_state ? get_cindex_from_window(task_state, window) : -2;
   if(ending_task) {
      if(!end_task(task, NULL))
         return; // keep window open so the task can be closed
   } else {
      if(task_state && task_state->process->window == windowIndex)
         task_state->process->window = -1;

      if(get_current_task_state() != task_state)
         do_callback = true; // don't notify for windows task itself closed
   }

   if(windowIndex == getSelectedWindowIndex())
      setSelectedWindowIndex(-1);

   window->closed = true;

   // remove from parent's child list
   if(window->parent) {
      gui_window_t *p = window->parent;
      for(int i = 0; i < p->child_count; i++) {
         if(p->children[i] == window) {
            p->children[i] = NULL;
            break;
         }
      }
      window->parent = NULL;
   }

   for(int i = 0; i < CMD_HISTORY_LENGTH; i++) {
      free((uint32_t)&window->cmd_history[i][0], TEXT_BUFFER_LENGTH);
      window->cmd_history[i] = NULL;
   }
   if(task_state && task_state->process && window->framebuffer) {
      // unmap from process (identity mapped)
      map_size(task_state->process->page_dir, (uint32_t)window->framebuffer, (uint32_t)window->framebuffer, window->framebuffer_size, 0, 1, 0);
   }
   free((uint32_t)window->framebuffer, window->width*(window->height-TITLEBAR_HEIGHT)*2);
   window->framebuffer = NULL;

   // free window objects
   for(int i = 0; i < window->window_object_count; i++) {
      windowobj_free(window->window_objects[i]);
      window->window_objects[i] = NULL;
   }

   // free state
   if(window->state != NULL) {
      if(window->state_free != NULL) {
         window->state_free(window, NULL);
      } else {
         free((uint32_t)window->state, window->state_size);
         window->state = NULL;
      }
   }

   // remove from render order
   for(int i = 0; i < windowCount; i++) {
      if(!render_order[i]) break;
      if(render_order[i] == window) {
         for(int j = i; j < windowCount-1; j++) {
            render_order[j] = render_order[j+1];
         }
         render_order[windowCount-1] = NULL;
         break;
      }
   }

   if(render_order[0] && !render_order[0]->closed && !render_order[0]->minimised)
      setSelectedWindowIndex(get_window_index_from_pointer(render_order[0]));

   // take children with them
   for(int i = 0; i < window->child_count; i++) {
      gui_window_t *child = (gui_window_t*)window->children[i];
      if(child == NULL || child->closed) continue;
      window_close(get_window_index_from_pointer(child), false);
   }

   // call close func if set
   if(do_callback && task_state && window->close_func != NULL && task != get_current_task()) {
      debug_printf("Calling close func for window %i\n", windowIndex);
      uint32_t *args = malloc(sizeof(uint32_t) * 1);
      args[0] = cindex;
      wm_call_subroutine(task_state, "close", (uint32_t)(window->close_func), args, 1);
   }
}

bool window_init(gui_window_t *window) {
   strcpy(window->title, "Window");
   window->x = gui_get_width()/16;
   window->y = gui_get_height()/16;
   window->width = 355;
   window->height = 265;
   window->text_buffer[0] = '\0';
   window->text_index = 0;
   window->text_x = getFont()->padding;
   window->text_y = getFont()->padding;
   window->needs_redraw = true;
   window->active = false;
   window->minimised = false;
   window->closed = false;
   window->dragged = false;
   window->bgcolour = wm_settings.default_window_bgcolour;
   window->txtcolour = wm_settings.default_window_txtcolour;
   window->state = NULL;
   window->state_free = NULL;
   window->resizable = true;
   window->resized = false;
   window->disabled = false;
   window->toolbar_pos = -1;
   window->scrolledY = 0;
   window->scrollable_content_height = 0;
   window->scrollbar = NULL;
   window->hovering = false;
   window->child_count = 0;
   window->parent = NULL;
   window->read_task = -1;

   window_resetfuncs(window);
   // no window objects
   window->window_object_count = 0;
   for(int i = 0; i < 20; i++)
      window->window_objects[i] = NULL;
   
   window->cmd_history[0] = malloc(CMD_HISTORY_LENGTH*TEXT_BUFFER_LENGTH);
   for(int i = 0; i < CMD_HISTORY_LENGTH; i++) {
      if(i > 0)
         window->cmd_history[i] = window->cmd_history[0] + TEXT_BUFFER_LENGTH*i;
      window->cmd_history[i][0] = '\0';
   }
   window->cmd_history_pos = -1;

   window->framebuffer_size = window->width*(window->height-TITLEBAR_HEIGHT)*2;
   window->framebuffer = malloc(window->framebuffer_size);
   if(window->framebuffer == NULL) return false;
   
   surface_t surface;
   surface.width = window->width;
   surface.height = window->height - TITLEBAR_HEIGHT;
   surface.pitch = window->width;
   surface.buffer = (uint32_t)window->framebuffer;
   window->surface = surface;

   window_clearbuffer(window, wm_settings.default_window_bgcolour);

   return true;
}

void window_resetfuncs(gui_window_t *window) {
   // reset all functions
   window->keypress_func = &window_term_keypress;
   window->draw_func = &window_term_draw;
   window->checkcmd_func = &window_term_checkcmd;

   // other functions without default behaviour
   window->keyrelease_func = NULL;
   window->click_func = NULL;
   window->drag_func = NULL;
   window->resize_func = NULL;
   window->release_func = NULL;
   window->hover_func = NULL;
   window->close_func = NULL;
   window->rightclick_func = NULL;
   window->mouseout_func = NULL;
}

void window_removefuncs(gui_window_t *window) {
   // reset all functions
   window->keypress_func = NULL;
   window->keyrelease_func = NULL;
   window->draw_func = NULL;
   window->checkcmd_func = NULL;

   // other functions without default behaviour
   window->click_func = NULL;
   window->drag_func = NULL;
   window->resize_func = NULL;
   window->release_func = NULL;
   window->hover_func = NULL;
   window->close_func = NULL;
   window->rightclick_func = NULL;
   window->mouseout_func = NULL;
}

// adds new window and selects it
int windowmgr_add() {
   int index = getFirstFreeIndex();
   if(index == -1) {
   
      if(windowCount < MAX_WINDOWS) {
         windowCount++;
         index = windowCount - 1;
      } else {
         debug_writestr("Couldn't create window\n");
         return -1;
      }
   }
      
   if(window_init(&gui_windows[index])) {
      setSelectedWindowIndex(index);
      int x = gui_get_width()/16 + index * 30;
      int y = gui_get_height()/16 + index * 30;
      if(x > gui_windows[index].width - 20)
         x = 20;
      if(y > gui_windows[index].height - TOOLBAR_HEIGHT - 20)
         y = 20;
      gui_windows[index].x = x;
      gui_windows[index].y = y;

      toolbar_draw();
      menubar_draw();

      return index;
   } else {
      strcpy((char*)gui_windows[index].title, "ERROR");
      gui_windows[index].closed = true;
      debug_writestr("Couldn't create window\n");
      return -1;
   }

}

// hide cursor if it overlaps with region about to be drawn to screen
bool cursor_hide_region(int x, int y, int width, int height) {
   if(!gui_cursor_shown || surface.buffer == (uint32_t)draw_buffer)
      return false;
   if(cursor_oldx >= x + width || cursor_oldx + getFont()->width <= x
   || cursor_oldy >= y + height || cursor_oldy + getFont()->height <= y)
      return false;
   gui_cursor_restore_bg();
   return true;
}

void cursor_show(bool hidden) {
   if(!hidden) return;
   gui_cursor_save_bg();
   gui_cursor_draw();
}

void window_draw_outline(gui_window_t *window, bool occlude) {
   if(window->closed || window->minimised) return;

   if(occlude) {
      for(int i = 0; i < getWindowCount(); i++) {
         if(render_order[i] == NULL)
            continue;
         if(render_order[i] == window)
            break;
         if(render_order[i]->minimised)
            continue;
         
         gui_window_t *win = render_order[i];
         if(window->x - 1 < win->x + win->width
         && window->x + window->width + 1 > win->x
         && window->y - 1 < win->y + win->height
         && window->y + window->height + 1 > win->y)
            return;
      }
   }

   // covers titlebar, outline & drop shadow
   bool cursor_hidden = cursor_hide_region(window->x - 1, window->y - 1, window->width + 3, window->height + 3);

   // titlebar

   int btnWidth = getFont()->width + getFont()->padding*4;
   int closeX = window->x + window->width - (btnWidth + getFont()->padding*2);
   int minimiseX = closeX - (btnWidth + getFont()->padding*2);
   int shadingWidth = minimiseX - window->x - 8;

   // centered text
   int titleWidth = font_width(strlen(window->title));
   int titleX = window->x + window->width/2 - titleWidth/2;
   int titleY = window->y + (TITLEBAR_HEIGHT - getFont()->height)/2;

   uint16_t c1;
   uint16_t c2;
   if(wm_settings.theme == 1) {
      // gradient titlebar
      c1 = wm_settings.titlebar_colour;
      c2 = wm_settings.titlebar_colour2;
      if(!window->active) {
         c1 = rgb16_lighten(c1, 100);
         c2 = rgb16_lighten(c2, 100);
      }
      draw_rect_gradient(&surface, c1, c2, window->x, window->y, window->width, TITLEBAR_HEIGHT, wm_settings.titlebar_gradientstyle);
   } else {
      // classic titlebar
      c1 = wm_settings.titlebar_colour;
      if(!window->active)
         c1 = rgb16_lighten(c1, 126);
      draw_rect(&surface, c1, window->x, window->y, window->width, TITLEBAR_HEIGHT);
      // shading
      c2 = COLOUR_LIGHT_GREY;
      if(!window->active)
         c2 = rgb16_lighten(c2, 110);
      for(int i = 0; i < TITLEBAR_HEIGHT-7; i++)
         draw_line(&surface, (i%2)==0 ? c2 : rgb16_lighten(c1, 62), window->x+4, window->y+4+i, false, shadingWidth);
      // rectangle behind title
      draw_rect(&surface, rgb16_lighten(c1, 50), titleX-6, window->y+3, titleWidth+12, getFont()->height+getFont()->padding*2+2);
   }
   // draw text
   draw_string(&surface, window->title, rgb16(210,210,210), titleX, titleY+1); // shadow
   draw_string(&surface, window->title, 0, titleX, titleY);
   if(window->active) {
      // draw underline
      draw_line(&surface, rgb16_darken(c2, 50), titleX, titleY+getFont()->height+1, false, titleWidth);
      draw_line(&surface, rgb16_lighten(c1, 50), titleX, titleY+getFont()->height+2, false, titleWidth);
   }

   // titlebar buttons
   draw_unfilledrect(&surface, c2, closeX, window->y+3, btnWidth, TITLEBAR_HEIGHT - 6);
   draw_unfilledrect(&surface, c2, minimiseX, window->y+3, btnWidth, TITLEBAR_HEIGHT - 6);
   draw_rect(&surface, window->active ? rgb16_lighten(c1, 80) : c1, closeX+1, window->y+4, btnWidth-2, TITLEBAR_HEIGHT - 8);
   draw_rect(&surface, window->active ? rgb16_lighten(c1, 80) : c1, minimiseX+1, window->y+4, btnWidth-2, TITLEBAR_HEIGHT - 8);
   draw_char(&surface, 0, COLOUR_DARK_GREY, closeX+getFont()->padding*2, titleY);
   draw_char(&surface, '_', COLOUR_DARK_GREY, minimiseX+getFont()->padding*2, titleY-1);

   draw_line(&surface, rgb16(170,170,170), window->x, window->y+TITLEBAR_HEIGHT-1, false, window->width);

   if(window != selectedWindow) {
      draw_dottedrect(&surface, rgb16(80, 80, 80), window->x-1, window->y-1, window->width+2, window->height+2, NULL, false);
   } else {
      // drop shadow if selected
      draw_line(&surface, COLOUR_DARK_GREY, window->x+window->width+1, window->y+3, true, window->height-1);
      draw_line(&surface, COLOUR_DARK_GREY, window->x+3, window->y+window->height+1, false, window->width-1);

      // outline
      draw_unfilledrect(&surface, gui_rgb16(80,80,80), window->x - 1, window->y - 1, window->width + 2, window->height + 2);
   }

   cursor_show(cursor_hidden);
}

void window_draw_content_region(gui_window_t *window, int offsetX, int offsetY, int width, int height, bool do_occlude) {
   if(window->framebuffer == NULL) return;
   if(window->closed || window->minimised) return;

   int winX = window->x;
   int winY = window->y + TITLEBAR_HEIGHT;
   int winW = window->width;
   int winH = window->height - TITLEBAR_HEIGHT;

   // clamp drawing to window content bounds
   int maxX = offsetX + width;
   int maxY = offsetY + height;
   if(offsetX < 0) offsetX = 0;
   if(offsetY < 0) offsetY = 0;
   if(maxX > winW) maxX = winW;
   if(maxY > winH) maxY = winH;
   if(offsetX >= maxX || offsetY >= maxY) return;
   width = maxX - offsetX;
   height = maxY - offsetY;

   uint16_t *fb = gui_get_framebuffer();
   uint16_t *winfb = window->framebuffer;

   bool cursor_hidden = cursor_hide_region(winX + offsetX, winY + offsetY, width, height);

   bool occlude = false;
   for(int i = 0; do_occlude && i < MAX_WINDOWS && render_order[i] != NULL; i++) {
      if(render_order[i] == window)
         break;
      
      gui_window_t *win = render_order[i];
      if(win->minimised) continue;
      if(!(window->x >= win->x + win->width
         || window->x + window->width + 2 <= win->x
         || window->y >= win->y + win->height
         || window->y + window->height + 2 <= win->y)) {
         occlude = true;
         break;
      }
   }

   if(!occlude && offsetX == 0 && width == winW) {
      // fast path: copy entire row
      for(int y = offsetY; y < maxY; y++) {
         int screenY = winY + y;
         memcpy_fast(&fb[screenY * surface.pitch + winX], &winfb[y * winW], width * sizeof(uint16_t));
      }
   } else {
      // general case: copy pixel by pixel
      for(int y = offsetY; y < maxY; y++) {
         int screenY = winY + y;
         for(int x = offsetX; x < maxX; x++) {
            int screenX = winX + x;

            bool occluded = false;
            for(int i = 0; do_occlude && i < MAX_WINDOWS && render_order[i] != NULL; i++) {
               if(render_order[i] == window) {
                  break;
               }
               
               gui_window_t *win = render_order[i];
               if(win->minimised) continue;
               if(screenX >= win->x - 1 && screenX < win->x + win->width + 1
               && screenY >= win->y - 1 && screenY < win->y + win->height + 1) {
                  occluded = true;
                  break;
               }
            }

            if(occluded)
               continue;

            int screenIndex = screenY * surface.pitch + screenX;
            int winIndex = y * winW + x;

            fb[screenIndex] = winfb[winIndex];
         }
      }
   }

   cursor_show(cursor_hidden);
}

void window_draw_content(gui_window_t *window, bool occlude) {
   window_draw_content_region(window, 0, 0, window->width, window->height - TITLEBAR_HEIGHT, occlude);
}

void window_disable(gui_window_t *window) {
   for(int y = 0; y < window->height - TITLEBAR_HEIGHT; y+=2) {
      for(int x = 0; x < window->width; x+=2) {
         window->framebuffer[y * window->width + x] = rgb16(150, 150, 150);
      }
   }
   window->disabled = true;
}
 
void window_draw(gui_window_t *window, bool occlude) {
   if(window->closed || window->minimised) return;

   if(window->resized) {
      draw_dottedrect(&surface, COLOUR_LIGHT_GREY, window->x, window->y, window->width, window->height, NULL, false);
      return;
   }

   if(window->needs_redraw || window == selectedWindow) {
      // call window objects draw funcs
      for(int i = 0; i < window->window_object_count; i++) {
         windowobj_t *wo = window->window_objects[i];
         if(wo != NULL) wo->draw_func(wo);
      }
      // draw window content/framebuffer
      window_draw_content(window, occlude);

      window->needs_redraw = false;
   }

   if(window == selectedWindow && window->draw_func != NULL)
      (*(window->draw_func))(window);

   if(window->dragged)
      draw_dottedrect(&surface, COLOUR_LIGHT_GREY, window->x, window->y, window->width, window->height, NULL, false);
}

void windowmgr_about() {
   default_menu->menuselected = -1;
   default_menu->menuhovered = -1;

   wm_queue_launch("/sys/about.elf", true, NULL, 0, false, true);
}

void windowmgr_getproperties() {
   default_menu->menuselected = -1;
   default_menu->menuhovered = -1;

   // show selected window settings 
   gui_window_t *selected = getSelectedWindow();
   if(!selected) {
      // get system settings
      wm_queue_launch("/sys/settings.elf", true, NULL, 0, true, false);
      return;
   }
   int new = windowmgr_add();
   if(new < 0 || !window_settings_init(getWindow(new), selected)) {
      debug_printf("Settings init failed\n");
      if(new >= 0) window_close(new, false);
      return;
   }
}

// focus new window
void windowmgr_focus_swap(gui_window_t *window) {
   gui_window_t *old_window = selectedWindow;
   if(window == old_window) return;

   setSelectedWindowIndex(get_window_index_from_pointer(window));

   if(old_window)
      window_draw_outline(old_window, false);

   if(window) {
      // draw new window on top
      window_draw_outline(window, false);
      window_draw(window, false);
   }
   
   if(old_window) {
      // redraw region under window outline + shadow
      int x = old_window->x, y = old_window->y, w = old_window->width, h = old_window->height;
      windowmgr_redraw_rect((rect_t){.x = x-1, .y = y-1, .w = w+2, .h = 1}); // top
      windowmgr_redraw_rect((rect_t){.x = x-1, .y = y-1, .w = 1, .h = h+2}); // left
      windowmgr_redraw_rect((rect_t){.x = x+w, .y = y-1, .w = 2, .h = h+3}); // right + shadow
      windowmgr_redraw_rect((rect_t){.x = x-1, .y = y+h, .w = w+3, .h = 2}); // bottom + shadow
   }

   toolbar_draw();
   menubar_draw();
}

void window_minimise(gui_window_t *window, bool minimise) {
   if(minimise) {
      if(!window->minimised) {
         window->minimised = true;
         if(window->active) {
            setSelectedWindowIndex(-1);
            menubar_draw();
         }
         toolbar_draw();
         rect_t old_bounds = {.w = window->width + 3, .h = window->height + 3, .x = window->x - 1, .y = window->y - 1};
         windowmgr_redraw_rect(old_bounds);
      }
   } else {
      // focus
      if(window->active) return; // already focused
      if(window->minimised) {
         window->minimised = false;
         if(selectedWindow) {
            windowmgr_focus_swap(window);
         } else {
            setSelectedWindowIndex(get_window_index_from_pointer(window));
            window_draw_outline(window, false);
            window_draw_content(window, false);
            menubar_draw();
            toolbar_draw();
         }
      } else {
         windowmgr_focus_swap(window);
      }
   }
}

void windowmgr_minimiseselected() {
   default_menu->menuselected = -1;
   default_menu->menuhovered = -1;

   // minimise selected window
   if(!selectedWindow) return; // no window selected
   window_minimise(selectedWindow, true);
}


void windowmgr_closeselected() {
   default_menu->menuselected = -1;
   default_menu->menuhovered = -1;

   // close selected window
   if(gui_selected_window == -1) return; // no window selected
   window_close(gui_selected_window, true);
   gui_redrawall();
}

void windowmgr_taskmanager() {
   default_menu->menuselected = -1;
   default_menu->menuhovered = -1;

   wm_queue_launch("/sys/taskmgr.elf", true, NULL, 0, true, false);
}

void windowmgr_init() {
   // init windowmgr settings
   strcpy(wm_settings.desktop_bgimg, "/bmp/bg16.bmp");
   strcpy(wm_settings.font_path, "/font/7.fon");

   // assigned fixed memory for MAX_WINDOWS windows for now for simplicity
   gui_windows = malloc(sizeof(gui_window_t) * MAX_WINDOWS);
   outline_buffer = malloc(sizeof(int) * (surface.width + surface.height + 4));
   memset(gui_windows, 0, sizeof(gui_window_t) * MAX_WINDOWS);
   for(int i = 0; i < MAX_WINDOWS; i++)
      render_order[i] = NULL;
   // Init with one (debug) window
   window_init(&gui_windows[0]);
   strcpy(gui_windows[0].title, "Debug Log");
   gui_windows[0].active = true;
   gui_windows[0].x = 100;
   gui_windows[0].y = 100;
   setSelectedWindowIndex(0);
   windowCount++;
   render_order[0] = &gui_windows[0];
   window_draw_outline(&gui_windows[0], false);
   window_draw(&gui_windows[0], true);

   // set up default menu
   default_menu = (windowobj_t*)malloc(sizeof(windowobj_t));
   windowobj_init(default_menu, &surface);
   default_menu->type = WO_MENU;
   default_menu->visible = false;
   default_menu->width = 80;
   default_menu->menuitems = malloc(sizeof(windowobj_menu_t) * 10);

   strcpy(default_menu->menuitems[0].text, "About");
   default_menu->menuitems[0].func = &windowmgr_about;
   default_menu->menuitems[0].disabled = false;

   strcpy(default_menu->menuitems[1].text, "Settings");
   default_menu->menuitems[1].func = &windowmgr_getproperties;
   default_menu->menuitems[1].disabled = false;

   strcpy(default_menu->menuitems[2].text, "Close");
   default_menu->menuitems[2].func = &windowmgr_closeselected;
   default_menu->menuitems[2].disabled = false;

   strcpy(default_menu->menuitems[3].text, "TaskMgr");
   default_menu->menuitems[3].func = &windowmgr_taskmanager;
   default_menu->menuitems[3].disabled = false;

   default_menu->menuitem_count = 4;
   windowobj_menu_resize(default_menu);

   // set up app button
   app_button = (windowobj_t*)malloc(sizeof(windowobj_t));
   windowobj_init(app_button, &surface);
   app_button->type = WO_BUTTON;
   app_button->x = surface.width - 42;
   app_button->y = surface.height - 20;
   app_button->width = 40;
   app_button->height = 20;
   app_button->text = "Apps";

   // set up menubar
   menubar_init();
}

void toolbar_draw() {
   bool cursor_hidden = cursor_hide_region(0, surface.height-TOOLBAR_HEIGHT, surface.width, TOOLBAR_HEIGHT);

   if(wm_settings.theme == 1) {
      // gradient toolbar
      draw_rect_gradient(&surface, wm_settings.titlebar_colour, wm_settings.titlebar_colour2, 0, surface.height-TOOLBAR_HEIGHT, surface.width, TOOLBAR_HEIGHT, wm_settings.titlebar_gradientstyle);
   } else {
      // classic toolbar
      gui_drawrect(COLOUR_TOOLBAR, 0, surface.height-TOOLBAR_HEIGHT, surface.width, TOOLBAR_HEIGHT);
   }

   int toolbarPos = 0;
   // padding = 2px
   for(int i = 0; i < getWindowCount(); i++) {
      if(getWindow(i)->closed) continue;

      int bg = rgb16(230, 230, 230);
      int bg2 = rgb16(210, 210, 210);
      int fg = COLOUR_DARK_GREY;
      if(getWindow(i)->minimised) {
         bg = COLOUR_TOOLBAR_ENTRY;
         bg2 = rgb16(100, 100, 100);
         fg = COLOUR_WHITE;
      }
      if(getWindow(i)->active) {
         bg = COLOUR_WHITE;
         bg2 = COLOUR_LIGHTLIGHT_GREY;
         fg = COLOUR_BLACK;
      }
      int displayedChars = strlen(getWindow(i)->title);
      if(displayedChars > (TOOLBAR_ITEM_WIDTH-20)/font_width(1))
         displayedChars = (TOOLBAR_ITEM_WIDTH-20)/font_width(1);
      if(displayedChars > 10)
         displayedChars = 10;
      int textWidth = font_width(displayedChars);
      int textX = TOOLBAR_ITEM_WIDTH/2 - textWidth/2;
      char text[11];
      strcpy_fixed(text, getWindow(i)->title, displayedChars);
      int itemX = TOOLBAR_PADDING+toolbarPos*(TOOLBAR_ITEM_WIDTH+TOOLBAR_PADDING);
      int itemY = surface.height-(TOOLBAR_ITEM_HEIGHT+TOOLBAR_PADDING);
      if(wm_settings.theme == 1) {
         draw_rect_gradient(&surface, bg, bg2, itemX, itemY, TOOLBAR_ITEM_WIDTH, TOOLBAR_ITEM_HEIGHT, windowmgr_get_settings()->titlebar_gradientstyle);
      } else {
         gui_drawrect(bg, itemX, itemY, TOOLBAR_ITEM_WIDTH, TOOLBAR_ITEM_HEIGHT);
      }
      int textY = itemY+(TOOLBAR_ITEM_HEIGHT - getFont()->height)/2;
      gui_writestrat(text, fg, itemX+textX, textY);
      // if selected, draw underline
      if(getWindow(i)->active)
         draw_line(&surface, rgb16(220, 220, 220), itemX + textX - 4, textY + getFont()->height + 2, false, font_width(strlen(text)) + 8);
      draw_line(&surface, COLOUR_TOOLBAR_BORDER, itemX, itemY + TOOLBAR_ITEM_HEIGHT, false, TOOLBAR_ITEM_WIDTH);
      getWindow(i)->toolbar_pos = toolbarPos;
      toolbarPos++;
   }
   windowobj_draw(app_button);

   cursor_show(cursor_hidden);
}

typedef struct wm_menubar_menu_t wm_menubar_menu_t;

// menu
typedef struct wm_menubar_menu_t {
   windowobj_t button;
   windowobj_t menu;
   windowobj_menu_t menuitems[MENUBAR_MAX_MENU_SIZE];
   void(*open_func)(wm_menubar_menu_t*); // called when menu is opened
} wm_menubar_menu_t;

// overall menubar struct
typedef struct wm_menubar_t {
   int menu_count;
   wm_menubar_menu_t menus[MENUBAR_MAX_ITEMS];
} wm_menubar_t;

static wm_menubar_menu_t sys_menu; // always displayed
static wm_menubar_t desktop_menubar = {.menu_count = 0};
static wm_menubar_t default_menubar = {.menu_count = 0};

static wm_menubar_t *active_menubar = NULL; // menubar that open_menu/hovered_menu point into
static wm_menubar_menu_t *open_menu = NULL;
static wm_menubar_menu_t *hovered_menu = NULL;

static void menubar_hide_open_menu() {
   open_menu->menu.menuhovered = -1;
   open_menu->menu.menuselected = -1;
   open_menu->menu.visible = false;
   open_menu = NULL;
}

static void menubar_close() {
   if(!open_menu) return;
   menubar_hide_open_menu();
   gui_redrawall();
}

// called when menu button clicked
static void menubar_menu_open(void *wo) {
   windowobj_t *w = wo;
   wm_menubar_menu_t *menu = w->userstate;
   bool visible = menu->menu.visible;
   menubar_close();
   menu->menu.visible = !visible;
   if(menu->menu.visible) {
      open_menu = menu;
      if(menu->open_func)
         menu->open_func(menu);
      gui_draw();
   }
}

wm_menubar_menu_t *menubar_add_menu(wm_menubar_t *menubar, char *name) {
   if(menubar->menu_count == MENUBAR_MAX_ITEMS)
      return NULL;

   int y = 0;

   wm_menubar_menu_t *menu = &menubar->menus[menubar->menu_count];
   menu->open_func = NULL;

   windowobj_t *button = &menu->button;
   windowobj_init(button, &surface);
   button->y = y;
   button->width = max(50, font_str_width(name) + MENUBAR_PADDING*2);
   button->height = 18;
   button->text = name;
   button->click_func = &menubar_menu_open;
   button->userstate = menu;
   if(menubar->menu_count > 0)
      button->bordered = false;

   windowobj_t *menu_wo = &menu->menu;
   windowobj_init(menu_wo, &surface);
   menu_wo->type = WO_MENU;
   menu_wo->y = MENUBAR_HEIGHT;
   menu_wo->width = MENUBAR_MENU_WIDTH;
   menu_wo->height = 100;
   menu_wo->visible = false;
   menu_wo->menuitem_count = 0;
   menu_wo->menuitems = menu->menuitems;

   menubar->menu_count++;
   return menu;
}

void menubar_menu_add_item(wm_menubar_menu_t *menu, void *func, char *text) {
   if(menu->menu.menuitem_count == MENUBAR_MAX_MENU_SIZE)
      return;

   windowobj_menu_t *item = &menu->menuitems[menu->menu.menuitem_count++];
   item->disabled = false;
   strcpy(item->text, text);
   item->func = func;
   windowobj_menu_resize(&menu->menu);
}

static void windowmgr_launch_apps();

static void windowmgr_launch_settings() {
   wm_queue_launch("/sys/settings.elf", true, NULL, 0, true, false);
}

static void windowmgr_launch_filemgr() {
   wm_queue_launch("/sys/files.elf", true, NULL, 0, true, false);
}

static void windowmgr_launch_usrterm() {
   wm_queue_launch("/sys/term.elf", true, NULL, 0, true, false);
}

static void windowmgr_launch_desktop() {
   wm_queue_launch("/sys/desktop.elf", true, NULL, 0, true, false);
}

static void windowmgr_quitselected() {
   if(!selectedWindow) return;
   int task = get_task_from_window(gui_selected_window);
   if(task < 0) return;
   end_task(task, NULL);
}

static void windowmgr_select_window() {
   if(!open_menu) return;
   int task = get_task_from_window(gui_selected_window);
   if(task < 0) return;
   int index = open_menu->menu.menuhovered - 4;
   if(index < -1) return;
   task_state_t *task_state = &gettasks()[task];
   process_t *process = task_state->process;
   if(!process || process->window < 0) return;
   gui_window_t *window = getWindow(process->window);
   if(index == -1) {
      window_minimise(window, false);
   } else {
      if(index >= window->child_count) return;
      gui_window_t *w = window->children[index];
      if(w)
         window_minimise(w, false);
   }
}

void menubar_open_window_menu(wm_menubar_menu_t *menu) {
   // populate 'window' menu
   menu->menu.menuitem_count = 3;
   menu->menu.width = MENUBAR_MENU_WIDTH;
   windowobj_menu_resize(&menu->menu);
   
   if(gui_selected_window < 0) return;
   int task = get_task_from_window(gui_selected_window);
   if(task < 0) return;
   task_state_t *task_state = &gettasks()[task];
   process_t *process = task_state->process;
   if(!process || process->window < 0) return;
   gui_window_t *window = getWindow(process->window);
   if(window->closed) return;
   char buf[256];
   sprintf(buf, "%s%s%s", window->title, window->active?" (selected)":"", window->minimised?" (minimised)":"");
   menubar_menu_add_item(menu, &windowmgr_select_window, buf);
   for(int i = 0; i < window->child_count; i++) {
      gui_window_t *w = window->children[i];
      if(!w) continue;
      sprintf(buf, "%s%s%s", w->title, w->active?" (selected)":"", w->minimised?" (minimised)":"");
      menubar_menu_add_item(menu, &windowmgr_select_window, buf);
   }
}

void menubar_init() {
   // init system menu
   windowobj_init(&sys_menu.button, &surface);
   windowobj_init(&sys_menu.menu, &surface);
   sys_menu.open_func = NULL;

   sys_menu.menu.menuitems = sys_menu.menuitems;
   sys_menu.menu.menuitem_count = 0;
   sys_menu.menu.type = WO_MENU;
   sys_menu.menu.visible = false;
   sys_menu.menu.width = 100;
   sys_menu.menu.y = MENUBAR_HEIGHT;

   sys_menu.button.width = max(50, font_str_width("f3sys") + MENUBAR_PADDING*2);
   sys_menu.button.height = 18;
   sys_menu.button.text = "f3sys";
   sys_menu.button.y = 0;
   sys_menu.button.click_func = &menubar_menu_open;

   menubar_menu_add_item(&sys_menu, &windowmgr_about, "About");
   menubar_menu_add_item(&sys_menu, &windowmgr_launch_settings, "Settings");
   menubar_menu_add_item(&sys_menu, &windowmgr_launch_apps, "Apps");
   menubar_menu_add_item(&sys_menu, &windowmgr_launch_filemgr, "FileMgr");
   menubar_menu_add_item(&sys_menu, &windowmgr_taskmanager, "TaskMgr");
   menubar_menu_add_item(&sys_menu, &windowmgr_launch_usrterm, "UsrTerm");

   // init desktop menubar
   desktop_menubar.menus[0] = sys_menu;
   desktop_menubar.menus[0].button.userstate = &desktop_menubar.menus[0];
   desktop_menubar.menu_count++;
   wm_menubar_menu_t *menu = menubar_add_menu(&desktop_menubar, "desktop");
   menu->button.underline = true;
   menubar_menu_add_item(menu, &windowmgr_launch_desktop, "Open");

   // init default menubar
   default_menubar.menus[0] = sys_menu;
   default_menubar.menus[0].button.userstate = &default_menubar.menus[0];
   default_menubar.menu_count++;
   menu = menubar_add_menu(&default_menubar, "App");
   menu->button.underline = true;
   menubar_menu_add_item(menu, &windowmgr_quitselected, "Quit");

   menu = menubar_add_menu(&default_menubar, "Window");
   menu->open_func = &menubar_open_window_menu;
   menubar_menu_add_item(menu, &windowmgr_minimiseselected, "Minimise");
   menubar_menu_add_item(menu, &windowmgr_closeselected, "Close");
   menubar_menu_add_item(menu, NULL, "");
}

wm_menubar_t *menubar_current() {
   wm_menubar_t *menubar = &desktop_menubar;
   if(selectedWindow) {
      int task = get_task_from_window(gui_selected_window);
      if(task >= 0) {
         process_t *process = gettasks()[task].process;
         if(process) {
            default_menubar.menus[1].button.text = getWindow(process->window)->title;
            menubar = &default_menubar;
         }
      }
   }

   if(menubar != active_menubar) {
      if(open_menu) {
         wm_menubar_menu_t *menu = open_menu;
         menubar_hide_open_menu();
         windowmgr_redraw_rect((rect_t){menu->menu.x, menu->menu.y, menu->menu.width, menu->menu.height});
      }
      if(hovered_menu) {
         hovered_menu->button.hovering = false;
         hovered_menu->button.clicked = false;
         hovered_menu = NULL;
      }
      active_menubar = menubar;
   }

   return menubar;
}

void menubar_draw() {
   bool cursor_hidden = cursor_hide_region(0, 0, surface.width, MENUBAR_HEIGHT);
   
   if(wm_settings.theme == 1) {
      // gradient toolbar
      draw_rect_gradient(&surface, wm_settings.titlebar_colour, wm_settings.titlebar_colour2, 0, 0, surface.width, MENUBAR_HEIGHT - 1, wm_settings.titlebar_gradientstyle);
   } else {
      // classic toolbar
      gui_drawrect(wm_settings.titlebar_colour, 0, 0, surface.width, MENUBAR_HEIGHT - 1);
   }
   draw_line(&surface, wm_settings.titlebar_colour2, 0, MENUBAR_HEIGHT - 1, false, surface.width);

   wm_menubar_t *menubar = menubar_current();

   int x = MENUBAR_PADDING;

   for(int i = 0; i < menubar->menu_count; i++) {
      windowobj_t *button = &menubar->menus[i].button;
      button->x = x;
      button->width = max(50, font_str_width(button->text) + MENUBAR_PADDING*2);
      windowobj_draw(button);
      windowobj_t *menu = &menubar->menus[i].menu;
      menu->x = x;
      windowobj_draw(menu);

      x += button->width + MENUBAR_PADDING;
   }

   cursor_show(cursor_hidden);
}

static bool wm_wo_overlaps(windowobj_t *wo, int x, int y) {
   return x >= wo->x && x < wo->x + wo->width && y >= wo->y && y < wo->y + wo->height;
}

static void wm_wo_mouse_handle(windowobj_t *wo, wm_event_type_t e, int x, int y) {
   if(e == MOUSE_CLICK) {
      windowobj_click(wo, x-wo->x, y-wo->y);
      windowobj_draw(wo);
   }
   if(e == MOUSE_HOVER) {
      windowobj_hover(wo, x-wo->x, y-wo->y);
   }
   if(e == MOUSE_RELEASE) {
      windowobj_release(wo, x-wo->x, y-wo->y);
   }
}

static bool menubar_mouse_event(wm_event_type_t e, int x, int y) {
   wm_menubar_t *menubar = menubar_current();

   // event outside menubar
   if(y >= MENUBAR_HEIGHT) {
      if(open_menu) {
         windowobj_t *menu_wo = &open_menu->menu;
         if(wm_wo_overlaps(menu_wo, x, y)) {
            // event within menu
            wm_wo_mouse_handle(menu_wo, e, x, y);
            if(e == MOUSE_RELEASE) {
               menubar_close();
            }
         } else if(e == MOUSE_CLICK || e == MOUSE_RIGHTCLICK) {
            menubar_close();
         } else if(e == MOUSE_HOVER) {
            menu_wo->hovering = false;
            windowobj_draw(menu_wo);
         }
         return true;
      } else if(hovered_menu) {
         hovered_menu->button.hovering = false;
         windowobj_draw(&hovered_menu->button);
         hovered_menu = NULL;
      }
      return false;
   }

   bool found = false;
   for(int i = 0; i < menubar->menu_count; i++) {
      windowobj_t *button = &menubar->menus[i].button;
      if(wm_wo_overlaps(button, x, y)) {
         found = true;
         wm_wo_mouse_handle(button, e, x, y);
         hovered_menu = &menubar->menus[i];
      } else if(button->hovering || button->clicked) {
         if(button->hovering)
            hovered_menu = NULL;
         button->hovering = false;
         button->clicked = false;
         windowobj_draw(button);
      }
      windowobj_t *menu_wo = &menubar->menus[i].menu;
      if(!menu_wo->visible) continue;
      if(wm_wo_overlaps(menu_wo, x, y)) {
         wm_wo_mouse_handle(menu_wo, e, x, y);
      } else {
         menu_wo->hovering = false;
         menu_wo->clicked = false;
         windowobj_draw(menu_wo);
      }
   }

   if(!found && open_menu && (e == MOUSE_CLICK || e == MOUSE_RIGHTCLICK))
      menubar_close();

   return false;
}

bool wm_wo_mouse_event(wm_event_type_t e, int x, int y) {
   if(menubar_mouse_event(e, x, y))
      return true;

   // check 'desktop objects' - wm owned windowobjs
   if(e == MOUSE_CLICK) {

      // menubar
      if(y < MENUBAR_HEIGHT)
         return true;

      // check default right click menu
      if(default_menu->visible) {
         if(wm_wo_overlaps(default_menu, x, y)) {
            // clicked on menu
            windowobj_click(default_menu, 0, 0); // unused x, y
            default_menu->visible = false;
            default_menu->clicked = false;
            gui_redrawall();
            return true;
         } else {
            // clicked elsewhere (hide)
            default_menu->visible = false;
            gui_redrawall();
            return true;
         }
      }

      // check app btn
      if(wm_wo_overlaps(app_button, x, y)) {
         app_button->clicked = true;
         wm_draw();
         return true;
      }

   }

   if(e == MOUSE_HOVER) {
      // check desktop windowobjects
      if(default_menu->visible) {
         // check if within menu
         if(wm_wo_overlaps(default_menu, x, y)) {
            int relX_wo = x - default_menu->x;
            int relY_wo = y - default_menu->y;
            if(default_menu->hover_func)
               default_menu->hover_func(default_menu, relX_wo, relY_wo);
         } else {
            if(default_menu->menuhovered != -1 || default_menu->hovering) {
               default_menu->menuhovered = -1;
               default_menu->hovering = false;
               windowobj_draw(default_menu);
            }
         }
         return true;
      }

      // app button
      if(wm_wo_overlaps(app_button, x, y)) {
         if(!app_button->hovering) {
            app_button->hovering = true;
            wm_draw();
         }
         return true;
      } else {
         if(app_button->hovering) {
            app_button->hovering = false;
            wm_draw();
         }
         return false;
      }
   }

   if(e == MOUSE_RIGHTCLICK) {
      // menubar
      if(y < MENUBAR_HEIGHT)
         return true;

      // draw menu
      default_menu->visible = true;
      app_button->visible = true;
      default_menu->x = x;
      default_menu->y = y;
      if(default_menu->y > surface.height - default_menu->height - TOOLBAR_HEIGHT) {
         // menu would be off screen, move up
         default_menu->y = surface.height - default_menu->height - TOOLBAR_HEIGHT;
      }
   }

   return false;
}

bool wm_wo_keyboard_event(wm_event_type_t e, uint16_t c) {
   if(e != KEY_PRESS) return false;

   if(default_menu->visible) {
      windowobj_keydown(default_menu, c);
      return true;
   }

   return false;
}

extern char scan_to_char(int scan_code, bool caps);

void windowmgr_swap_window() {
   // find next in render order after the selected window, wrapping to the first
   gui_window_t *target = NULL;
   gui_window_t *first = NULL;
   bool foundsel = selectedWindow == NULL; // nothing selected: take the first
   for(int i = 0; i < windowCount; i++) {
      if(render_order[i] == NULL) continue;
      if(!first)
         first = render_order[i];
      if(render_order[i] == selectedWindow) {
         foundsel = true;
      } else if(foundsel) {
         target = render_order[i];
         break;
      }
   }
   if(!target)
      target = first;

   if(target)
      window_minimise(target, false);
}

bool keyboard_shift = false;
bool keyboard_caps = false;
bool keyboard_alt = false;

bool wm_check_shortcut(int scan_code) {
   if(!keyboard_alt) return false;

   // alt+w
   if(scan_to_char(scan_code, false) == 'w') {
      window_close(gui_selected_window, true);
      gui_redrawall();
      return true;
   }
   // alt+up
   if(scan_code == 72) {
      gui_window_t *first = render_order[0];
      if(first && !first->active && first->minimised)
         window_minimise(first, false);
      return true;
   }
   // alt+down
   if(scan_code == 80) {
      if(selectedWindow)
         window_minimise(selectedWindow, true);
      return true;
   }
   // alt+space
   if(scan_to_char(scan_code, false) == ' ') {
      windowmgr_launch_apps();
      return true;
   }
   // alt+tab
   if(scan_code == 0x0F) {
      windowmgr_swap_window();
      return true;
   }
   // alt+number
   if(scan_to_char(scan_code, false) >= '1' && scan_to_char(scan_code, false) <= '9') {
      int num = scan_to_char(scan_code, false) - '1';
      int count = 0;
      for(int i = 0; i < getWindowCount(); i++) {
         if(getWindow(i)->closed) continue;
         if(count == num) {
            window_minimise(getWindow(i), false);
            return true;
         }
         count++;
      }
   }
   // alt+esc
   if(scan_code == 0x01) {
      wm_queue_launch("/sys/taskmgr.elf", true, NULL, 0, true, false);
      return true;
   }
   // alt+t
   if(scan_to_char(scan_code, false) == 't') {
      wm_queue_launch("/sys/term.elf", true, NULL, 0, true, false);
      return true;
   }
   return false;
}

uint16_t wm_get_char(int scan_code) {
   // convert to char
   uint16_t c = (uint16_t)scan_to_char(scan_code, keyboard_shift^keyboard_caps);
   if(c == 0) { // special chars
      if(scan_code == 14)
         c = KEY_BACKSPACE;
      if(scan_code == 1)
         c = KEY_ESCAPE;
      if(scan_code == 28)
         c = KEY_RETURN;
      if(scan_code == 0x1D)
         c = KEY_CONTROL;
      if(scan_code == 0x2A || scan_code == 0x36) 
         c = KEY_SHIFT;
      if(scan_code == 72)
         c = KEY_UPARROW;
      if(scan_code == 80)
         c = KEY_DOWNARROW;
      if(scan_code == 75)
         c = KEY_LEFTARROW;
      if(scan_code == 77)
         c = KEY_RIGHTARROW;
   }
   return c;
}

void windowmgr_keypress(int scan_code) {
   // check desktop window objects
   bool released = scan_code & 0x80;
   scan_code = scan_code & 0x7F;

   // check modifiers
   if(scan_code == 0x38) {
      // alt
      keyboard_alt = !released;
   }

   // check for wm shortcuts (alt+key)
   if(!released && wm_check_shortcut(scan_code))
      return;

   if(scan_code == 0x2A || scan_code == 0x36)
      keyboard_shift = !released;

   if(released && scan_code == 0x3A)
      keyboard_caps = !keyboard_caps;

   uint16_t c = wm_get_char(scan_code);
   if(c == 0) return;

   if(wm_wo_keyboard_event(released ? KEY_RELEASE : KEY_PRESS, c))
      return;

   if(selectedWindow == NULL) return;
   
   if(released)
      window_keyrelease(selectedWindow, c);
   else   
      window_keypress(selectedWindow, c);
}

bool clicked_on_window(int index, int x, int y) {
   if(index < 0) return false;
   gui_window_t *window = getWindow(index);
   if(!window || window->closed) return false;

   if(x >= TOOLBAR_PADDING+window->toolbar_pos*(TOOLBAR_ITEM_WIDTH+TOOLBAR_PADDING) && x <= TOOLBAR_PADDING+window->toolbar_pos*(TOOLBAR_ITEM_WIDTH+TOOLBAR_PADDING)+TOOLBAR_ITEM_WIDTH
   && y >= (int)surface.height-(TOOLBAR_ITEM_HEIGHT+TOOLBAR_PADDING) && y <= (int)surface.height-TOOLBAR_PADDING) {
      // clicked on window's icon in toolbar, maximise
      if(window->minimised) {
         window_minimise(window, false);
      } else if(window->active) {
         window_minimise(window, true);
      } else {
         window->needs_redraw = true;
         windowmgr_focus_swap(window);
      }
      return true;
   } else if(!window->minimised && x >= window->x && x <= window->x + window->width && y >= window->y && y <= window->y + window->height) {
      // clicked within window bounds

      int relX = x - window->x;
      int relY = y - window->y;

      int btnWidth = getFont()->width + getFont()->padding*4;
      int closeX = window->width - (btnWidth + getFont()->padding*2);
      int minimiseX = closeX - (btnWidth + getFont()->padding*2);

      // close
      if(relY < TITLEBAR_HEIGHT
      && relX > closeX
      && relX < closeX + btnWidth) {
         window_close(index, true);
         gui_redrawall();
         return true;
      }

      // minimise
      if(relY < TITLEBAR_HEIGHT
      && relX > minimiseX
      && relX < minimiseX + btnWidth) {
         window_minimise(window, true);
         return true;
      }

      window->needs_redraw = true;
      bool alreadyselected = window == selectedWindow;
      windowmgr_focus_swap(window);

      if(relY < TITLEBAR_HEIGHT) {
         window->dragged = true;
         window->drag_x = window->x;
         window->drag_y = window->y;
         outline_saved = false;
         dragged_window = window;
         return true;
      }

      if(alreadyselected) { // only send window click event when already the focused window
         window_click(window, relX, relY-TITLEBAR_HEIGHT);
         pressed_window = window;
      }
      return true;
   }

   return false;
}

void windowmgr_dragged(int relX, int relY) {
   gui_window_t *window = getSelectedWindow();
   if(selectedWindow == NULL || !selectedWindow->active) return;

   if(!selectedWindow->dragged && !selectedWindow->resized) {
      if(relX == 0 && relY == 0) return;
   
      int windowX = gui_mouse_x - selectedWindow->x;
      int windowY = gui_mouse_y - (selectedWindow->y + TITLEBAR_HEIGHT);

      // check scrollbar
      if(selectedWindow->scrollbar != NULL && selectedWindow->scrollbar->visible) {
         windowobj_t *scroller = selectedWindow->scrollbar->children[0];
         if(scroller->clicked) {
            int woX = windowX - selectedWindow->scrollbar->x;
            int woY = windowY - selectedWindow->scrollbar->y;
            windowobj_dragged(scroller, woX, woY, relX, relY);
            return;
         }
      }

      bool in_window = !(windowX < 0 || windowY < 0 || windowX >= selectedWindow->width || windowY >= selectedWindow->height - TITLEBAR_HEIGHT);
      if(in_window)
         window_drag(window, windowX, windowY, relX, relY);
      return;
   }

   gui_cursor_restore_bg();

   int wx = window->dragged ? window->drag_x : window->x;
   int wy = window->dragged ? window->drag_y : window->y;

   // restore dotted outline
   if(outline_saved)
      draw_dottedrect(&main_surface, COLOUR_LIGHT_GREY, wx, wy, window->width, window->height, outline_buffer, true);

   if(window->dragged) {
      wx += relX;
      wy -= relY;
      if(wx < 1)
         wx = 1;
      if(wx + window->width + 2 > (int)surface.width)
         wx = surface.width - window->width - 2;
      if(wy < MENUBAR_HEIGHT)
         wy = MENUBAR_HEIGHT;
      if(wy + window->height > (int)surface.height - TOOLBAR_HEIGHT)
         wy = surface.height - window->height - TOOLBAR_HEIGHT;
      window->drag_x = wx;
      window->drag_y = wy;
   }
   if(selectedWindow->resized) {
      selectedWindow->width += relX;
      selectedWindow->height -= relY;
   }

   window->needs_redraw = true;

   // draw dotted outline
   draw_dottedrect(&main_surface, COLOUR_LIGHT_GREY, wx, wy, window->width, window->height, outline_buffer, false);
   outline_saved = true;
   gui_cursor_save_bg();
   gui_cursor_draw(); 
}

static void windowmgr_launch_apps() {
   // check if already exists
   for(int i = 0; i < windowCount; i++) {
      if(gui_windows[i].closed) continue;
      if(strequ(gui_windows[i].title, "Apps")) {
         if(i == getSelectedWindowIndex())
            window_minimise(selectedWindow, true);
         else
            window_minimise(&gui_windows[i], false);
         return;
      }
   }
   wm_queue_launch("/sys/apps.elf", true, NULL, 0, false, true);
}

bool windowmgr_click(int x, int y) {
   pressed_window = NULL;
   if(wm_wo_mouse_event(MOUSE_CLICK, x, y))
      return true;

   if(gui_get_cursor() == CURSOR_RESIZE && selectedWindow) { // hovering on edge of selected window
      selectedWindow->resized = true;
      outline_saved = false;
      return true;
   }

   if(clicked_on_window(getSelectedWindowIndex(), x, y))
      return true; // clicked on selected
      
   gui_window_t *clicked = NULL;

   // check other windows
   for(int i = 0; i < getWindowCount(); i++) {
      if(render_order[i] == NULL) continue;
      int index = get_window_index_from_pointer(render_order[i]);

      if(clicked_on_window(index, x, y)) {
         clicked = getWindow(index);
         break;
      }
   }
   
   if(clicked) {
      // focused
      if(clicked == selectedWindow && !clicked->dragged) {
         clicked->dragged = true;
         clicked->drag_x = clicked->x;
         clicked->drag_y = clicked->y;
         outline_saved = false;
         dragged_window = clicked;
      }
      return true;
   }
   
   windowmgr_focus_swap(NULL);
   return false;
}

void windowmgr_release(int x, int y) {
   gui_window_t *pressed = pressed_window;
   pressed_window = NULL;

   if(wm_wo_mouse_event(MOUSE_RELEASE, x, y))
      return;

   if(app_button->clicked) {
      app_button->clicked = false;
      if(x >= app_button->x && x <= app_button->x + app_button->width
      && y >= app_button->y && y <= app_button->y + app_button->height) {
         windowmgr_launch_apps();
         return;
      }
      wm_draw();
      return;
   }

   // only the window that got the press gets the release, not one the press just focused
   if(pressed && !pressed->closed && !pressed->minimised) {
      int relX = gui_mouse_x - pressed->x;
      int relY = gui_mouse_y - pressed->y - TITLEBAR_HEIGHT;
      window_release(pressed, relX, relY);
   }

   gui_window_t *window = dragged_window ? dragged_window : getSelectedWindow();
   dragged_window = NULL;
   bool redraw = false;
   if(!window) return;
   if(window->dragged) {
      redraw = outline_saved;
      window->dragged = false;
      window->x = window->drag_x;
      window->y = window->drag_y;
   }
   if(window->resized) {
      redraw = true;
      window_resize(NULL, window, window->width, window->height, true);
      window->resized = false;
   }
   outline_saved = false;
   if(redraw)
      gui_redrawall();
}

void windowmgr_rightclick(int x, int y) {
   menubar_current();
   if(y < MENUBAR_HEIGHT || open_menu) {
      wm_wo_mouse_event(MOUSE_RIGHTCLICK, x, y);
      return;
   }

   if(selectedWindow) {
      int relX = x - selectedWindow->x;
      int relY = y - (selectedWindow->y + TITLEBAR_HEIGHT);

      if(relX >= 0 && relX < selectedWindow->width
      && relY + TITLEBAR_HEIGHT >= 0 && relY < selectedWindow->height - TITLEBAR_HEIGHT) {
         // right click within selected window
         if(relY >= 0 && window_rightclick(selectedWindow, relX, relY))
            return; // overrides default right click behaviour
      } else {
         windowmgr_focus_swap(NULL);
      }
   }

   if(wm_wo_mouse_event(MOUSE_RIGHTCLICK, x, y))
      return;
}

void windowmgr_draw() {

   gui_cursor_restore_bg();

   for(int i = getWindowCount()-1; i >= 0; i--) {
      if(render_order[i] == NULL) continue;
      window_draw(render_order[i], true);
   }

   windowobj_draw(default_menu);
   toolbar_draw();
   menubar_draw();

   gui_cursor_save_bg();
   gui_cursor_draw();
}

void windowmgr_redrawall() {
   // draw to buffer
   surface_t screen = surface; // save old surface
   surface.buffer = (uint32_t)draw_buffer;
   surface.pitch = surface.width; // tightly packed

   gui_clear(gui_bg);
   desktop_draw();
   // draw all windows
   // save render order
   gui_window_t *saved_order[MAX_WINDOWS];
   memcpy(saved_order, render_order, sizeof(gui_window_t*)*MAX_WINDOWS);
   for(int i = getWindowCount()-1; i >= 0; i--) {
      if(saved_order[i] == NULL) continue;
      saved_order[i]->needs_redraw = true;
      window_draw_outline(saved_order[i], false);
      window_draw(saved_order[i], false);
      kernel_yield_if_blocking();
   }
   toolbar_draw();
   menubar_draw();

   // copy to display
   surface = screen; // restore
   uint16_t *fb = (uint16_t*)surface.buffer;
   if(surface.pitch == surface.width) {
      memcpy_fast(fb, draw_buffer, sizeof(uint16_t) * surface.width * surface.height);
   } else {
      for(int y = 0; y < surface.height; y++)
         memcpy_fast(&fb[y * surface.pitch], &draw_buffer[y * surface.width], sizeof(uint16_t) * surface.width);
   }

   gui_cursor_shown = false;
   gui_cursor_save_bg();
   gui_cursor_draw();
}

void windowmgr_redraw_rect(rect_t clip) {
   menubar_current();

   // draw to buffer
   surface_t screen = surface; // save old surface
   surface.buffer = (uint32_t)draw_buffer;
   surface.pitch = surface.width; // tightly packed

   draw_set_clip(clip);

   gui_clear(gui_bg);
   desktop_draw();
   // draw all windows
   // save render order
   gui_window_t *saved_order[MAX_WINDOWS];
   memcpy(saved_order, render_order, sizeof(gui_window_t*)*MAX_WINDOWS);
   for(int i = getWindowCount()-1; i >= 0; i--) {
      gui_window_t *window = saved_order[i];
      if(!window) continue;
      // outer bounds include outline (-1) and shadow (+2)
      if(window->x - 1 >= clip.x + clip.w || window->x + window->width + 2 <= clip.x
      || window->y - 1 >= clip.y + clip.h || window->y + window->height + 2 <= clip.y)
         continue;
      window_draw_outline(window, false);
      window_draw_content_region(window, clip.x - window->x, clip.y - (window->y + TITLEBAR_HEIGHT), clip.w, clip.h, false); // bounds clamped to window in func
   }
   toolbar_draw();
   menubar_draw();

   draw_clear_clip();
   // copy just the rect to display
   surface = screen; // restore
   if(!clip_rect(&surface, &clip.x, &clip.y, &clip.w, &clip.h)) return;
   bool cursor_hidden = cursor_hide_region(clip.x, clip.y, clip.w, clip.h);
   uint16_t *fb = (uint16_t*)surface.buffer;
   for(int y = clip.y; y < clip.y + clip.h; y++)
      memcpy_fast(&fb[y * surface.pitch + clip.x], &draw_buffer[y * surface.width + clip.x], sizeof(uint16_t) * clip.w);
   cursor_show(cursor_hidden);
}


void desktop_init() {
   // load add window icon
   int size;
   if(!icon_window) {
      icon_window = fs_read_file_kernel("/bmp/window16.bmp", &size);
      if(!icon_window) {
         debug_writestr("Window icon not found\n");
         return;
      }
   }
   if(!icon_files) {
      icon_files = fs_read_file_kernel("/bmp/files16.bmp", &size);
      if(!icon_files) {
         debug_writestr("Files icon not found\n");
         return;
      }
   }

   if(!gui_bgimage) {
      gui_bgimage = fs_read_file_kernel(wm_settings.desktop_bgimg, &size);
      if(!gui_bgimage) {
         debug_writestr("BG not found\n");
         return;
      }

      gui_bgimage_size = size;
      gui_bg = bmp_get_colour(gui_bgimage, 0, 0);
   }

   wm_settings.desktop_enabled = true;
}

void desktop_setbgimg(uint8_t *img, int size) {
   if(gui_bgimage)
      free((uint32_t)gui_bgimage, gui_bgimage_size);
   gui_bgimage = img;
   gui_bgimage_size = size;
   gui_bg = bmp_get_colour(gui_bgimage, 0, 0);
}

void desktop_draw() {
   if(!wm_settings.desktop_enabled) return;

   if(wm_settings.desktop_bgimg_enabled) {
      int32_t width = bmp_get_width(gui_bgimage);
      int32_t height = bmp_get_height(gui_bgimage);
      int x = (surface.width - width) / 2;
      int y = ((surface.height - TOOLBAR_HEIGHT - MENUBAR_HEIGHT) - height) / 2;
      bmp_draw(gui_bgimage, &surface,x, y, 0, 1);
   }

   int x = 10;
   int y = 30;
   int textx = x + (bmp_get_width(icon_files) - font_width(strlen("FileMgr"))) / 2;
   bmp_draw(icon_files, &surface,x, y, 1, 1);
   y += 6 + bmp_get_height(icon_files);
   draw_string(&surface, "FileMgr", 0, textx, y+1);
   draw_string(&surface, "FileMgr", 0xFFFF, textx, y);
   y += 14 + getFont()->height;
   textx = x + (bmp_get_width(icon_window) - font_width(strlen("UsrTerm"))) / 2;
   bmp_draw(icon_window, &surface,x, y, 1, 1);
   y += 6 + bmp_get_height(icon_window);
   draw_string(&surface, "UsrTerm", 0, textx, y+1);
   draw_string(&surface, "UsrTerm", 0xFFFF, textx, y);
   y += 14 + getFont()->height;
   textx = x + (bmp_get_width(icon_window) - font_width(strlen("KTerm"))) / 2;
   bmp_draw(icon_window, &surface,x, y, 1, 1);
   y += 6 + bmp_get_height(icon_window);
   draw_string(&surface, "KTerm", 0, textx, y+1);
   draw_string(&surface, "KTerm", 0xFFFF, textx, y);
}

void desktop_click(int x, int y) {
   if(!wm_settings.desktop_enabled) return;

   int icony = 30;
   int iconheight = bmp_get_height(icon_window);

   // filemgr
   if(x >= 10 && y >= icony && x <= 60 && y <= icony + iconheight) {
      wm_queue_launch("/sys/files.elf", true, NULL, 0, true, false);
   }

   icony += 6+14 + iconheight + getFont()->height;
   iconheight = bmp_get_height(icon_window);

   // usr terminal
   if(x >= 10 && y >= icony && x <= 60 && y <= icony + iconheight) {
      wm_queue_launch("/sys/term.elf", true, NULL, 0, true, false);
   }

   icony += 6+14 + iconheight + getFont()->height;
   iconheight = bmp_get_height(icon_files);

   // kterm
   if(x >= 10 && y >= icony && x <= 60 && y <= icony + iconheight) {
      int w = windowmgr_add();
      if(w < 0) return;
      strcpy(getWindow(w)->title, "KTerm");
      window_draw_outline(getWindow(w), false);
      window_draw(getWindow(w), true);
   }
}

void windowmgr_mousemove(int x, int y) {
   if(wm_wo_mouse_event(MOUSE_HOVER, x, y)) return;

   if(selectedWindow) {
      // check if within selected window content

      int relX = x - selectedWindow->x;
      int relY = y - (selectedWindow->y + TITLEBAR_HEIGHT);

      if(selectedWindow->dragged || selectedWindow->resized) {
         gui_set_cursor(selectedWindow->resized ? CURSOR_RESIZE : CURSOR_DEFAULT);
         return; // skip hover functionality
      }

      if(relX > 0 && relX < selectedWindow->width
      && relY > 0 && relY < selectedWindow->height - TITLEBAR_HEIGHT) {
         selectedWindow->hovering = true;
         window_mousemove(selectedWindow, relX, relY);
      } else {
         if(selectedWindow->hovering) {
            // mouse out/unhover
            selectedWindow->hovering = false;
            window_mouseout(selectedWindow);
         }
      }

      // if just outside to the bottom right, display resize cursor
      if(selectedWindow->resizable && relX >= selectedWindow->width - 1 && relX < selectedWindow->width + 5
      && relY >= selectedWindow->height - 1 - TITLEBAR_HEIGHT && relY < selectedWindow->height + 5 - TITLEBAR_HEIGHT) {
         gui_set_cursor(CURSOR_RESIZE);
      } else {
         gui_set_cursor(CURSOR_DEFAULT);
      }

   } else {
      gui_set_cursor(CURSOR_DEFAULT);
   }
}

void windowmgr_scroll(int y, int w) {
   gui_window_t *window = w < 0 ? getSelectedWindow() : getWindow(w);
   if(!window || window->closed) return;
   int offsetY;
   if(w < 0) {
      offsetY = window->scrolledY + y; // delta y
   } else {
      offsetY = y; // absolute y
   }
   window_scroll_to(NULL, window, offsetY);
}

// just store index in struct lol(!) or at least do pointer maths
int get_window_index_from_pointer(gui_window_t *window) {
   if(!window) return -1;
   for(int i = 0; i < windowCount; i++) {
      if(window == &gui_windows[i]) return i;
   }
   return -1;
}

void window_resize(registers_t *regs, gui_window_t *window, int width, int height, bool callback) {
   width = max(width, 20);
   height = max(height, 20);
   int maxwidth = surface.width - 5;
   int maxheight = surface.height - TOOLBAR_HEIGHT - MENUBAR_HEIGHT - 5;
   int screenbottom = surface.height - TOOLBAR_HEIGHT - 5;
   width = min(width, maxwidth);
   height = min(height, maxheight);
   if(window->x + width > maxwidth)
      window->x = maxwidth - width + 2;
   if(window->y + height > screenbottom)
      window->y = screenbottom - height;
   if(window->y < MENUBAR_HEIGHT)
      window->y = MENUBAR_HEIGHT;

   uint16_t *old_framebuffer = window->framebuffer;
   uint32_t old_framebuffer_size = window->framebuffer_size;
   int old_width = window->surface.width;
   int old_height = window->surface.height;

   // unmap previous framebuffer
   int index = get_window_index_from_pointer(window);
   int task = get_task_from_window(index);
   task_state_t *task_state = &gettasks()[task];
   if(task > -1)
      map_size(task_state->process->page_dir, (uint32_t)window->framebuffer, (uint32_t)window->framebuffer, window->framebuffer_size, 0, 1, 0);

   window->framebuffer_size = width*(height-TITLEBAR_HEIGHT)*2;
   window->framebuffer = malloc(window->framebuffer_size);
   window->width = width;
   window->height = height;
   window->surface.buffer = (uint32_t)window->framebuffer;
   window->surface.width = width;
   window->surface.height = height - TITLEBAR_HEIGHT;
   window->surface.pitch = width;

   int scrollbarwidth = (window->scrollbar && window->scrollbar->visible) ? window->scrollbar->width : 0;

   int min_width = min(window->width, old_width-scrollbarwidth);
   int min_height = min(window->surface.height, old_height);
   window_clearbuffer(window, window->bgcolour);
   // copy window
   for(int x = 0; x < min_width; x++) {
      for(int y = 0; y < min_height; y++) {
         window->framebuffer[x + window->surface.width*y] = old_framebuffer[x + old_width*y];
      }
   }

   free((uint32_t)old_framebuffer, old_framebuffer_size);

   bool scrollerhidden = false; // scrollbar is now hidden after resize
   if(window->scrollbar != NULL) {
      // resize scrollbar
      window->scrollbar->x = width - 14;
      window->scrollbar->height = height - TITLEBAR_HEIGHT;
      int scrollareaheight = (height - TITLEBAR_HEIGHT - 28);
      windowobj_t *scroller = window->scrollbar->children[0];
      if(window->scrollable_content_height == 0)
         scroller->height = 0;
      else
         scroller->height = (scrollareaheight * window->scrollbar->height) / window->scrollable_content_height;
      if(scroller->height < 10) scroller->height = 10;
      if(scroller->height >= scrollareaheight) {
         scroller->height = scrollareaheight;
         scroller->y = 14;
      }
      windowobj_t *downbtn = window->scrollbar->children[2];
      downbtn->y = height - TITLEBAR_HEIGHT - 14;

      bool visible = window->scrollbar->visible;
      window->scrollbar->visible = window->height - TITLEBAR_HEIGHT < window->scrollable_content_height;
      if(visible && !window->scrollbar->visible)
         scrollerhidden = true;
   }

   // call resize func if exists
   if(callback && task > -1 && window->resize_func) {
      uint32_t *args = malloc(sizeof(uint32_t) * 4);
      args[3] = (uint32_t)window->framebuffer;
      args[2] = width - (window->scrollbar && window->scrollbar->visible ? 14 : 0);
      args[1] = height - TITLEBAR_HEIGHT;
      args[0] = get_cindex_from_window(task_state, window);
      map_size(task_state->process->page_dir, (uint32_t)window->framebuffer, (uint32_t)window->framebuffer, window->framebuffer_size, 1, 1, 0);
      if(regs)
         task_call_subroutine(regs, task_state, "resize", (uint32_t)(window->resize_func), args, 4);
      else
         wm_call_subroutine(task_state, "resize", (uint32_t)(window->resize_func), args, 4);
   }

   if(scrollerhidden) {
      // scrollbar has become hidden, scroll right to top
      // needs to be done after resize event as framebuffer changed
      window_scroll_to(regs, window, 0);
   }
}

windowmgr_settings_t *windowmgr_get_settings() {
   return &wm_settings;
}

// get 'cindex' of currently selected window
// main task window has index -1, children 0+, not found -2
int get_cindex(task_state_t *task) {
   int w = get_task_window(task->task_id); // main window of current task
   if(w < 0)
      return -2;

   gui_window_t *window = getWindow(w);
   if(window == NULL || window->closed)
      return -2;
   if(window == selectedWindow)
      return -1;
   for(int i = 0; i < window->child_count; i++) {
      gui_window_t *child = window->children[i];
      if(child && child->active && child == selectedWindow)
         return i;
   }
   return -2;
}

int get_cindex_from_window(task_state_t *task, gui_window_t *window) {
   int w = get_task_window(task->task_id); // main window of current task
   gui_window_t *mainwin = getWindow(w);
   if(w < 0)
      return -2;

   if(window == NULL || window->closed)
      return -2;
   if(window == mainwin)
      return -1; // main window
   for(int i = 0; i < mainwin->child_count; i++) {
      gui_window_t *child = mainwin->children[i];
      if(child && child->active && child == window)
         return i;
   }
   return -2;
}