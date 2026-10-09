#include "windowmgr.h"
#include "memory.h"
#include "window.h"
#include "tasks.h"
#include "draw.h"
#include "lib/string.h"
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

static int *outline_buffer;
static bool outline_saved = false;

static gui_window_t *dragged_window = NULL;

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

extern bool mouse_heldright;
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

extern bool cursor_resize;

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

// would be much better as a linked link
gui_window_t *render_order[100];

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
      for(int i = windowCount; i > 0; i--) {
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

   if(windowIndex == 0) {
      // debug window
      getWindow(windowIndex)->minimised = true;
      setSelectedWindowIndex(-1);
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

      return index;
   } else {
      strcpy((char*)gui_windows[index].title, "ERROR");
      gui_windows[index].closed = true;
      debug_writestr("Couldn't create window\n");
      return -1;
   }

}

extern bool gui_cursor_shown;
extern int gui_mouse_x;
extern int gui_mouse_y;
extern int cursor_oldx;
extern int cursor_oldy;

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
   if(window->minimised) return;

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
         c1 = rgb16_lighten(c1, 120);
      draw_rect(&surface, c1, window->x, window->y, window->width, TITLEBAR_HEIGHT);
      // shading
      c2 = COLOUR_LIGHT_GREY;
      if(!window->active)
         c2 = rgb16_lighten(c2, 80);
      for(int i = 0; i < TITLEBAR_HEIGHT-7; i++)
         draw_line(&surface, (i%2)==0?c2 : rgb16_lighten(c1, 50), window->x+4, window->y+4+i, false, shadingWidth);
      // rectangle behind title
      draw_rect(&surface, rgb16_lighten(c1, 50), titleX-6, window->y+3, titleWidth+12, getFont()->height+getFont()->padding*2+2);
   }
   // draw text
   draw_string(&surface, window->title, rgb16(210,210,210), titleX, titleY+1); // shadow
   draw_string(&surface, window->title, 0, titleX, titleY);
   if(window->active) {
      // draw underline
      draw_line(&surface, c2, titleX, titleY+getFont()->height+1, false, titleWidth);
      draw_line(&surface, rgb16_lighten(c1, 50), titleX, titleY+getFont()->height+2, false, titleWidth);
   }

   // titlebar buttons
   draw_unfilledrect(&surface, rgb16(220, 220, 220), closeX, window->y+3, btnWidth, TITLEBAR_HEIGHT - 6);
   draw_unfilledrect(&surface, rgb16(220, 220, 220), minimiseX, window->y+3, btnWidth, TITLEBAR_HEIGHT - 6);
   draw_rect(&surface, COLOUR_LIGHT_GREY, closeX+1, window->y+4, btnWidth-2, TITLEBAR_HEIGHT - 8);
   draw_rect(&surface, COLOUR_LIGHT_GREY, minimiseX+1, window->y+4, btnWidth-2, TITLEBAR_HEIGHT - 8);
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

void window_draw_content_region(gui_window_t *window, int offsetX, int offsetY, int width, int height) {
   if(window->framebuffer == NULL) return;

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
   for(int i = 0; i < 100 && render_order[i] != NULL; i++) {
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
            for(int i = 0; i < 100 && render_order[i] != NULL; i++) {
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

void window_draw_content(gui_window_t *window) {
   window_draw_content_region(window, 0, 0, window->width, window->height - TITLEBAR_HEIGHT);
}

void window_disable(gui_window_t *window) {
   for(int y = 0; y < window->height - TITLEBAR_HEIGHT; y+=2) {
      for(int x = 0; x < window->width; x+=2) {
         window->framebuffer[y * window->width + x] = rgb16(150, 150, 150);
      }
   }
   window->disabled = true;
}
 
void window_draw(gui_window_t *window) {

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
      window_draw_content(window);

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

void windowmgr_closeselected() {
   default_menu->menuselected = -1;
   default_menu->menuhovered = -1;

   // close selected window
   int index = getSelectedWindowIndex();
   if(index == -1) return; // no window selected
   window_close(index, true);
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
   for(int i = 0; i < 100; i++)
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
   window_draw(&gui_windows[0]);

   // set up default menu
   default_menu = (windowobj_t*)malloc(sizeof(windowobj_t));
   windowobj_init(default_menu, &surface);
   default_menu->type = WO_MENU;
   default_menu->visible = false;
   default_menu->width = 80;
   default_menu->height = 50;
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

   // set up app button
   app_button = (windowobj_t*)malloc(sizeof(windowobj_t));
   windowobj_init(app_button, &surface);
   app_button->type = WO_BUTTON;
   app_button->x = surface.width - 42;
   app_button->y = surface.height - 20;
   app_button->width = 40;
   app_button->height = 20;
   app_button->text = "Apps";
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

      int bg = COLOUR_LIGHT_GREY;
      int bg2 = rgb16(175, 175, 175);
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

extern char scan_to_char(int scan_code, bool caps);

void windowmgr_swap_window() {
   if(selectedWindow != NULL) {
      // find next in render order
      bool foundsel = false;
      bool found = false;
      int first = -1;
      for(int i = 0; i < windowCount; i++) {
         if(render_order[i] != NULL && first == -1)
            first = i;
         if(render_order[i] == selectedWindow) {
            foundsel = true;
         } else if(foundsel && render_order[i] != NULL) {
            setSelectedWindowIndex(get_window_index_from_pointer(render_order[i]));
            found = true;
            break;
         }
      }
      if(!found && first != -1)
         setSelectedWindowIndex(get_window_index_from_pointer(render_order[first]));
   } else {
      for(int i = 0; i < windowCount; i++) {
         if(render_order[i] != NULL) {
            setSelectedWindowIndex(get_window_index_from_pointer(render_order[i]));
            break;
         }
      }
   }

   if(selectedWindow)
      selectedWindow->minimised = false;

   gui_redrawall();
}

static void windowmgr_launch_apps();

bool keyboard_shift = false;
bool keyboard_caps = false;
bool keyboard_alt = false;

void windowmgr_keypress(int scan_code) {
   // check desktop window objects
   if(default_menu->visible) {
      windowobj_keydown(default_menu, scan_code);
      return;
   }

   bool released = scan_code & 0x80;
   scan_code = scan_code & 0x7F;

   // check modifiers
   if(scan_code == 0x38) {
      // alt
      keyboard_alt = !released;
   }
   if(keyboard_alt) {
      // alt+w
      if(scan_to_char(scan_code, false) == 'w') {
         window_close(gui_selected_window, true);
         gui_redrawall();
         keyboard_alt = false;
         return;
      }
      // alt+up
      if(scan_code == 72) {
         gui_window_t *first = render_order[0];
         if(first && !first->active && first->minimised) {
            first->minimised = false;
            setSelectedWindowIndex(get_window_index_from_pointer(first));
            gui_redrawall();
         }
         keyboard_alt = false;
         return;
      }
      // alt+down
      if(scan_code == 80) {
         if(selectedWindow) {
            selectedWindow->minimised = true;
            setSelectedWindowIndex(-1);
         }
         gui_redrawall();
         keyboard_alt = false;
         return;
      }
      // alt+space
      if(scan_to_char(scan_code, false) == ' ') {
         windowmgr_launch_apps();
         keyboard_alt = false;
         return;
      }
      // alt+tab
      if(scan_code == 0x0F) {
         windowmgr_swap_window();
         keyboard_alt = false;
         return;
      }
      // alt+number
      if(scan_to_char(scan_code, false) >= '1' && scan_to_char(scan_code, false) <= '9') {
         int num = scan_to_char(scan_code, false) - '1';
         int count = 0;
         for(int i = 0; i < getWindowCount(); i++) {
            if(getWindow(i)->closed) continue;
            if(count == num) {
               setSelectedWindowIndex(i);
               getWindow(i)->minimised = false;
               gui_redrawall();
               return;
            }
            count++;
         }
      }
      // alt+esc
      if(scan_code == 0x01) {
         wm_queue_launch("/sys/taskmgr.elf", true, NULL, 0, true, false);
         keyboard_alt = false;
         return;
      }
      // alt+t
      if(scan_to_char(scan_code, false) == 't') {
         wm_queue_launch("/sys/term.elf", true, NULL, 0, true, false);
         keyboard_alt = false;
         return;
      }
   }

   if(selectedWindow == NULL) return;

   if(scan_code == 0x2A || scan_code == 0x36)
      keyboard_shift = !released;

   // convert to char
   uint16_t c = (uint16_t)scan_to_char(scan_code, keyboard_shift^keyboard_caps);
   if(c == 0) { // special chars
      if(scan_code == 14) // backspace
         c = 0x08;
      if(scan_code == 1) // escape
         c = 0x1B;
      if(scan_code == 28) // return
         c = 0x0D;
      if(scan_code == 0x1D) // control
         c = 0x80;
      if(scan_code == 0x2A || scan_code == 0x36) // shift
         c = 0x81;
      if(scan_code == 72) // uparrow
         c = 0x100;
      if(scan_code == 80) // downarrow
         c = 0x101;
      if(scan_code == 75) // leftarrow
         c = 0x102;
      if(scan_code == 77) // rightarrow
         c = 0x103;
   }
   
   if(released) {
      // keyrelease func
      if(!selectedWindow->keyrelease_func) return;
      int taskIndex = get_task_from_window(getSelectedWindowIndex());
      if(taskIndex < 0) return;
      task_state_t *task = &gettasks()[taskIndex];

      uint32_t *args = malloc(sizeof(uint32_t) * 2);
      args[1] = c;
      args[0] = get_cindex(task);

      wm_call_subroutine(task, "keyrelease", (uint32_t)(selectedWindow->keyrelease_func), args, 2);
      return;
   }
   
   if(scan_code == 0x3A)
      keyboard_caps = true;

   // check if we have a window object selected
   for(int i = 0; i < selectedWindow->window_object_count; i++) {
      windowobj_t *wo = selectedWindow->window_objects[i];
      if(!wo->clicked) continue;

      windowobj_keydown((void*)wo, scan_code);
      windowobj_redraw((void*)selectedWindow, (void*)wo);
      return;
   }

   // otherwise, default behaviour
   switch(scan_code) {
      case 0x3A: // caps
         keyboard_caps = !keyboard_caps;
         break;
      default:
         // call window keypress func with char
         if(!selectedWindow->keypress_func) break;

         int taskIndex = get_task_from_window(gui_selected_window);

         if(taskIndex == -1 || selectedWindow->keypress_func == &window_term_keypress) {
            // launch into function directly as kernel
            (*(selectedWindow->keypress_func))(c, selectedWindow);
         } else {
            // run as task
            task_state_t *task = &gettasks()[taskIndex];
            uint32_t *args = malloc(sizeof(uint32_t) * 2);
            args[1] = c;
            args[0] = get_cindex(task);

            wm_call_subroutine(task, "keypress", (uint32_t)(selectedWindow->keypress_func), args, 2);
         }

         break;
   }
}

bool clicked_on_window(int index, int x, int y) {
   if(index < 0) return false;
   gui_window_t *window = getWindow(index);
   if(!window || window->closed) return false;

   if(x >= TOOLBAR_PADDING+window->toolbar_pos*(TOOLBAR_ITEM_WIDTH+TOOLBAR_PADDING) && x <= TOOLBAR_PADDING+window->toolbar_pos*(TOOLBAR_ITEM_WIDTH+TOOLBAR_PADDING)+TOOLBAR_ITEM_WIDTH
   && y >= (int)surface.height-(TOOLBAR_ITEM_HEIGHT+TOOLBAR_PADDING) && y <= (int)surface.height-TOOLBAR_PADDING) {
      // clicked on window's icon in toolbar, maximise
      if(window->minimised) {
         window->minimised = false;
         window->needs_redraw = true;
         setSelectedWindowIndex(index);
      } else if(window->active) {
         window->minimised = true;
         window->active = false;
         setSelectedWindowIndex(-1);
         gui_redrawall();
      } else {
         window->needs_redraw = true;
         setSelectedWindowIndex(index);
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
         window->minimised = true;
         setSelectedWindowIndex(-1);
         gui_redrawall();
         return true;
      }

      window->needs_redraw = true;
      bool alreadyselected = window == selectedWindow;
      setSelectedWindowIndex(index);

      if(relY < TITLEBAR_HEIGHT) {
         window->dragged = true;
         window->drag_x = window->x;
         window->drag_y = window->y;
         outline_saved = false;
         dragged_window = window;
         return true;
      }

      if(!alreadyselected) return true;

      // check if we clicked on window object
      relY -= TITLEBAR_HEIGHT;

      int clicked_index = -1;
      for(int i = 0; i < selectedWindow->window_object_count; i++) {
         windowobj_t *wo = selectedWindow->window_objects[i];
         if(relX >= wo->x && relX < wo->x + wo->width
         && relY >= wo->y && relY < wo->y + wo->height) {
            windowobj_click((void*)wo, relX - wo->x, relY - wo->y);
            windowobj_redraw((void*)selectedWindow, (void*)wo);
            clicked_index = i;
            break;
         }
      }
      
      // unclick others
      for(int i = 0; i < selectedWindow->window_object_count; i++) {
         if(i == clicked_index) continue;
         windowobj_t *wo = selectedWindow->window_objects[i];
         wo->clicked = false;
         for(int x = 0; x < wo->child_count; x++) {
            windowobj_t *child = wo->children[x];
            child->clicked = false;
         }
      }

      return true;

   }

   return false;
}

extern uint16_t *draw_buffer;

extern int gui_mouse_x;
extern int gui_mouse_y;

int clicked_x;
int clicked_y;
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
      if(!in_window) return;

      // check windowobjs
      for(int i = 0; i < selectedWindow->window_object_count; i++) {
         windowobj_t *wo = selectedWindow->window_objects[i];
         int woX = windowX - wo->x;
         int woY = windowY - wo->y;
         if(woX < 0 || woY < 0 || woX >= wo->width || woY >= wo->height || wo == selectedWindow->scrollbar)
            continue;
         windowobj_dragged(wo, woX, woY, relX, relY);
      }

      // call drag func
      if(selectedWindow->drag_func == NULL) return;
      int taskIndex = get_task_from_window(getSelectedWindowIndex());
      if(taskIndex == -1) {
         // call as kernel
         getSelectedWindow()->drag_func(windowX, windowY);
      } else {
         // calling function as task
         task_state_t *task = &gettasks()[taskIndex];
         uint32_t *args = malloc(sizeof(uint32_t) * 3);
         args[2] = windowX;
         args[1] = windowY;
         args[0] = get_cindex(task);
      
         wm_call_subroutine(task, "dragged", (uint32_t)(selectedWindow->drag_func), args, 3);
      
         return;
      }
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
      if(wy < 0)
         wy = 0;
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
         if(i == getSelectedWindowIndex()) {
            getSelectedWindow()->minimised = true;
            setSelectedWindowIndex(-1);
         } else {
            setSelectedWindowIndex(i);
            if(getSelectedWindow()->minimised)
               getSelectedWindow()->minimised = false;
         }
         gui_redrawall();
         return;
      }
   }
   wm_queue_launch("/sys/apps.elf", true, NULL, 0, false, true);
}

bool windowmgr_click(int x, int y) {
   clicked_x = x;
   clicked_y = y;

   gui_window_t *prevSelected = selectedWindow;

   if(cursor_resize && selectedWindow != NULL) {
      selectedWindow->resized = true;
      outline_saved = false;
      return false;
   }

   if(default_menu->visible) {
      // clicked on menu
      if(x >= default_menu->x && x <= default_menu->x + default_menu->width
      && y >= default_menu->y && y <= default_menu->y + default_menu->height) {
         windowobj_click(default_menu, 0, 0); // unused x, y
         default_menu->visible = false;
         gui_redrawall();
         return true;
      } else {
         default_menu->visible = false;
         gui_redrawall();
         return false;
      }
   }

   // clicked app btn
   if(x >= app_button->x && x <= app_button->x + app_button->width
      && y >= app_button->y && y <= app_button->y + app_button->height) {
      app_button->clicked = true;
      wm_draw();
      return true;
   }

   if(!clicked_on_window(getSelectedWindowIndex(), x, y)) {
      // check other windows
      setSelectedWindowIndex(-1);
      for(int i = 0; i < getWindowCount(); i++) {
         if(render_order[i] == NULL) continue;
         int index = get_window_index_from_pointer(render_order[i]);

         if(clicked_on_window(index, x, y)) break;
      }
      if(selectedWindow) {
         selectedWindow->dragged = true;
         selectedWindow->drag_x = selectedWindow->x;
         selectedWindow->drag_y = selectedWindow->y;
         dragged_window = selectedWindow;
      }
      gui_redrawall();
   }

   if(selectedWindow == NULL) return false;

   // only call click routine when already the focused window
   if(selectedWindow != prevSelected) return true;
   
   if(selectedWindow->click_func == NULL) return true;

   if(y < selectedWindow->y + TITLEBAR_HEIGHT) {
      // clicked titlebar, don't call routine
      return true;
   }

   // switch to task
   int taskIndex = get_task_from_window(getSelectedWindowIndex());
   if(taskIndex == -1) {
      // call as kernel
      getSelectedWindow()->click_func(x - selectedWindow->x, y - (selectedWindow->y + TITLEBAR_HEIGHT));
   } else {
      // calling as task
      task_state_t *task = &gettasks()[taskIndex];
      uint32_t *args = malloc(sizeof(uint32_t) * 3);
      args[2] = x - selectedWindow->x;
      args[1] = y - (selectedWindow->y + TITLEBAR_HEIGHT);
      args[0] = get_cindex(task);
      map(task->process->page_dir, (uint32_t)args, (uint32_t)args, 1, 1, 0);
      wm_call_subroutine(task, "click", (uint32_t)(selectedWindow->click_func), args, 3);
   }

   return true;
}

void windowmgr_release(int x, int y) {
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

   gui_window_t *window = dragged_window ? dragged_window : getSelectedWindow();
   dragged_window = NULL;
   bool redraw = false;
   if(!window) return;
   if(window->dragged) {
      redraw = true;
      window->dragged = false;
      window->x = window->drag_x;
      window->y = window->drag_y;
   }
   if(window->resized) {
      redraw = true;
      window_resize(NULL, window, window->width, window->height, true);
      window->resized = false;
   } else {
      window_release(window);
   }
   if(redraw)
      gui_redrawall();
}

void windowmgr_rightclick(int x, int y) {
   if(selectedWindow == NULL || !(x - selectedWindow->x >= 0 && x - selectedWindow->x < selectedWindow->width
   && y - selectedWindow->y >= 0 && y - selectedWindow->y < selectedWindow->height)) {
      setSelectedWindowIndex(-1);
      gui_redrawall();
   } else {
      // right click within selected window
      if(selectedWindow->rightclick_func != NULL) {
         // switch to task
         int taskIndex = get_task_from_window(getSelectedWindowIndex());
         if(taskIndex > -1) {
            task_state_t *task = &gettasks()[taskIndex];
            uint32_t *args = malloc(sizeof(uint32_t) * 3);
            args[2] = x - selectedWindow->x;
            args[1] = y - (selectedWindow->y + TITLEBAR_HEIGHT);
            args[0] = get_cindex(task);
            map(task->process->page_dir, (uint32_t)args, (uint32_t)args, 1, 1, 0);
            wm_call_subroutine(task, "rightclick", (uint32_t)(selectedWindow->rightclick_func), args, 3);
            return;
         }
      }
   }
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

void windowmgr_draw() {

   gui_cursor_restore_bg();

   for(int i = getWindowCount()-1; i >= 0; i--) {
      if(render_order[i] == NULL) continue;
      window_draw(render_order[i]);
   }

   windowobj_draw(default_menu);
   toolbar_draw();

   gui_cursor_save_bg();
   gui_cursor_draw();
}

void windowmgr_redrawall() {
   // draw to buffer
   surface_t screen = surface; // save old surface
   surface.buffer = (uint32_t)draw_buffer;
   surface.pitch = surface.width; // tightly packed

   extern uint16_t gui_bg;
   gui_clear(gui_bg);
   desktop_draw();
   // draw all windows
   for(int i = getWindowCount()-1; i >= 0; i--) {
      if(render_order[i] == NULL) continue;
      render_order[i]->needs_redraw = true;
      window_draw_outline(render_order[i], false);
      window_draw(render_order[i]);
      kernel_yield_if_blocking();
   }
   toolbar_draw();

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

extern uint16_t gui_bg;
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
      int y = ((surface.height - TOOLBAR_HEIGHT) - height) / 2;
      bmp_draw(gui_bgimage, &surface,x, y, 0, 1);
   }

   int x = 10;
   int y = 10;
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

   int icony = 10;
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
      strcpy(getWindow(w)->title, "KTerm");
      window_draw(getSelectedWindow());
   }
}

void windowmgr_mousemove(int x, int y) {
   bool cursor_old = cursor_resize;

   // check desktop windowobjects
   if(default_menu->visible) {
      // check if within menu
      int relX_wo = x - default_menu->x;
      int relY_wo = y - default_menu->y;
      if(relX_wo > 0 && relX_wo < default_menu->width
      && relY_wo > 0 && relY_wo < default_menu->height) {
         if(default_menu->hover_func)
            default_menu->hover_func(default_menu, relX_wo, relY_wo);
         return;
      } else {
         if(default_menu->menuhovered != -1 || default_menu->hovering) {
            default_menu->menuhovered = -1;
            default_menu->hovering = false;
            windowobj_draw(default_menu);
         }
      }
   }

   if(selectedWindow) {
      // check if within selected window content

      int relX = x - selectedWindow->x;
      int relY = y - (selectedWindow->y + TITLEBAR_HEIGHT);

      if(selectedWindow->dragged || selectedWindow->resized) {
         cursor_resize = selectedWindow->resized;
         return; // skip hover functionality
      }

      if(relX > 0 && relX < selectedWindow->width
      && relY > 0 && relY < selectedWindow->height - TITLEBAR_HEIGHT) {
         selectedWindow->hovering = true;
         // within current window, check window objects
         for(int i = 0; i < selectedWindow->window_object_count; i++) {
            windowobj_t *wo = selectedWindow->window_objects[i];
            int relX_wo = relX - wo->x;
            int relY_wo = relY - wo->y;
            bool inside = relX_wo > 0 && relX_wo < wo->width && relY_wo > 0 && relY_wo < wo->height;
        
            if(inside) {
               if(!wo->hovering || wo->type == WO_MENU || wo->type == WO_CANVAS || wo->type == WO_SCROLLBAR) {
                  //wo->hovering = true;
                  if(wo->hover_func)
                     wo->hover_func((void*)wo, relX_wo, relY_wo);
                  windowobj_redraw((void*)selectedWindow, (void*)wo);
               }
            } else {
               if(wo->hovering) {
                  wo->hovering = false;

                  for(int i = 0; i < wo->child_count; i++) {
                     ((windowobj_t*)(wo->children[i]))->hovering = false;
                  }

                  if(wo->draw_func)
                     wo->draw_func((void*)wo);
                  windowobj_redraw((void*)selectedWindow, (void*)wo);
               }
            }
         }

         // call hover function
         if(selectedWindow->hover_func) {
            int task = get_task_from_window(getSelectedWindowIndex());
            if(task < 0) return;
            task_state_t *task_state = &gettasks()[task];
            uint32_t *args = malloc(sizeof(uint32_t) * 3);
            args[2] = relX;
            args[1] = relY;
            args[0] = get_cindex(task_state);
            wm_call_subroutine(task_state, "hover", (uint32_t)(selectedWindow->hover_func), args, 3);
         }
      } else {
         if(selectedWindow->hovering) {
            // mouse out/unhover
            selectedWindow->hovering = false;
            if(!selectedWindow->resized && selectedWindow->mouseout_func) {
               // call mouseout routine
               int task = get_task_from_window(getSelectedWindowIndex());
               if(task < 0) return;
               task_state_t *task_state = &gettasks()[task];
               uint32_t *args = malloc(sizeof(uint32_t) * 1);
               args[0] = get_cindex(task_state);
               wm_call_subroutine(task_state, "mouseout", (uint32_t)(selectedWindow->mouseout_func), args, 1);
            }
         }
         // set all window objs as not hovered
         for(int i = 0; i < selectedWindow->window_object_count; i++) {
            windowobj_t *wo = selectedWindow->window_objects[i];
            if(wo->hovering) {
               wo->hovering = false;
               for(int i = 0; i < wo->child_count; i++) {
                  ((windowobj_t*)(wo->children[i]))->hovering = false;
               }
               if(wo->draw_func)
                  wo->draw_func((void*)wo);
               windowobj_redraw((void*)selectedWindow, (void*)wo);
            }
         }
      }

      // if just outside to the bottom right, display resize cursor
      if(selectedWindow->resizable && relX >= selectedWindow->width - 1 && relX < selectedWindow->width + 5
      && relY >= selectedWindow->height - 1 - TITLEBAR_HEIGHT && relY < selectedWindow->height + 5 - TITLEBAR_HEIGHT) {
         cursor_resize = true;
      } else {
         cursor_resize = false;
      }

   } else {
      cursor_resize = false;
   }

   if(cursor_old != cursor_resize) {
      // cursor changed
      gui_cursor_restore_bg();
      gui_cursor_draw();
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
   for(int i = 0; i < windowCount; i++) {
      if(window == &gui_windows[i]) return i;
   }
   return -1;
}

static inline int min(int a, int b) { return (a < b) ? a : b; }
void window_resize(registers_t *regs, gui_window_t *window, int width, int height, bool callback) {
   int maxheight = surface.height - TOOLBAR_HEIGHT - 5;
   if(height > maxheight) height = maxheight;
   int y_overflow = window->y + height - maxheight;
   if(y_overflow >= 0) {
      window->y -= y_overflow;
   }

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

void window_release(gui_window_t *window) {
   // call mouse release func if exists
   int index = get_window_index_from_pointer(window);
   int task = get_task_from_window(index);
   if(task > -1 && window->release_func) {
      task_state_t *task_state = &gettasks()[task];
      uint32_t *args = malloc(sizeof(uint32_t) * 3);
      args[2] = gui_mouse_x - window->x; // x relative to window content
      args[1] = gui_mouse_y - window->y - TITLEBAR_HEIGHT; // y relative to window content
      args[0] = get_cindex(task_state);
      wm_call_subroutine(task_state, "release", (uint32_t)(window->release_func), args, 3);
   }

   // check windowobjs
   for(int i = 0; i < window->window_object_count; i++) {
      windowobj_t *wo = window->window_objects[i];
      if(wo->clicked) {
         int relX = gui_mouse_x - window->x - wo->x;
         int relY = gui_mouse_y - window->y - TITLEBAR_HEIGHT - wo->y;
         if(windowobj_release((void*)wo, relX, relY)) return;
      }
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