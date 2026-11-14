// task manager

#include "lib/dialogs.h"
#include "prog.h"
#include "lib/stdio.h"
#include "../lib/string.h"
#include "../lib/maths.h"

int task_count;
api_task_t *tasks;
dialog_t *dialog;

int selected_task = -1;

void task_show_info(int index) {
   api_task_t *task = &tasks[index];
   selected_task = task->id;
   bool main_thread = task->id == task->parentid;
   dialog_get(dialog, "info_label")->visible = false;
   dialog_get(dialog, "info_canvas")->visible = true;

   label_t *label_id = dialog_get(dialog, "info_label_id")->data;
   if(main_thread)
      snprintf(label_id->label, sizeof(label_id->label), "Task ID: %i - UID: %u\n%s", task->id, task->uid, task->exe_path);
   else
      snprintf(label_id->label, sizeof(label_id->label), "Task ID: %i - UID: %u", task->id, task->uid);

   label_t *label_window = dialog_get(dialog, "info_label_window")->data;
   if(main_thread) { // main thread
      snprintf(label_window->label, sizeof(label_window->label), "Process UID: %u", task->process_uid);
   } else { // child
      snprintf(label_window->label, sizeof(label_window->label), "Task is child of task %i", task->parentid);
   }

   label_t *label_memory = dialog_get(dialog, "info_label_memory")->data;
   snprintf(label_memory->label, sizeof(label_memory->label), "Heap size: %i\nTicks: %u", task->heap_end - task->heap_start, task->ticks);

   ui_redraw(dialog->ui);
}

void task_click(wo_t *wo, int index, int window) {
   (void)window;
   menu_t *menu = wo->data;
   int taskid = strtoint(menu->items[index].text+strlen("Task "));
   for(int i = 0; i < task_count; i++) {
      if(tasks[i].id == taskid) {
         task_show_info(i);
         return;
      }
   }
}

void show_tasks() {
   task_count = get_tasks(NULL, 0);
   if(tasks)
      free(tasks);
   tasks = malloc(task_count*sizeof(api_task_t));
   int read = get_tasks(tasks, task_count);
   if(read < task_count)
      task_count = read;
   wo_t *menu_wo = dialog_get(dialog, "tasks_menu");
   menu_t *menu = menu_wo->data;
   menu->item_count = 0;
   char buffer[512];
   for(int i = 0; i < task_count; i++) {
      api_task_t *task = &tasks[i];
      if(!task->enabled) continue;
      sprintf(buffer, "Task %i ", task->id);
      if(task->parentid == task->id) {
         strcat(buffer, task->main_window_name);
         if(strlen(task->main_window_name) == 0)
            strcat(buffer, task->exe_path);
      } else {
         char buf2[16];
         sprintf(buf2, "(parent %i)", task->parentid);
         strcat(buffer, buf2);
      }
      if(task->paused)
         strcat(buffer, " <paused>");
      add_menu_item(menu_wo, buffer, &task_click);
   }
}

void launch_task_return(char *out, int window) {
   dialog_t *dialog = dialog_from_window(window);
   dialog->active = false;
   launch_task(out, 0, NULL, false, false, NULL);
   close_window(window);
}

void launch_task_callback(wo_t *wo, int window) {
   (void)wo;
   (void)window;
   dialog_filepicker("/sys", &launch_task_return);
}

static bool escalated = false;

void end_task_callback(wo_t *wo, int window) {
   (void)wo;
   (void)window;
   if(!escalated)
      escalated = escalate();
   if(!escalated) return;
   end_task(selected_task, -1);
   show_tasks();
   clear();
   ui_redraw(dialog->ui);
}

void refresh_callback(wo_t *wo, int window) {
   (void)wo;
   dialog_t *dialog = dialog_from_window(window);
   menu_t *menu = dialog_get(dialog, "tasks_menu")->data;
   int offset = menu->offset;
   menu->offset = 0;
   show_tasks();
   menu->offset = min(offset, menu->item_count - menu->shown_items);
   clear();
   ui_redraw(dialog->ui);
}

void close_callback(wo_t *wo, int window) {
   (void)wo;
   close_window(window);
   exit(0);
}

void get_key(uint32_t uid, char out[5]) {
   snprintf(out, 5, "%h", uid);
}

static bool monitor_open(dialog_t *m_dialog, int w, int tid) {
   return m_dialog->active && m_dialog->window == w && (int)m_dialog->state == tid;
}

void monitor_thread() {
   int w = create_window(360, 220);
   if(w < 0) _exit(1);
   int index = get_free_dialog();
   if(index < 0) {
      close_window(w);
      _exit(1);
   }
   dialog_t *m_dialog = get_dialog(index);
   if(!dialog_init(m_dialog, w)) {
      close_window(w);
      _exit(1);
   }
   int tid = get_taskid();
   m_dialog->state = (void*)tid; // set after dialog_init, which resets state
   dialog_set_title(m_dialog, "Task Monitor");

   int t = get_tasks(NULL, 0);
   api_task_t *tasks = malloc(sizeof(api_task_t)*t);
   int read = min(t, get_tasks(tasks, t));
   map_t map; // task uid->api_task_t*
   map_init(&map, read*2);
   for(int i = 0; i < read; i++) {
      if(!tasks[i].enabled) continue;
      char key[9];
      get_key(tasks[i].uid, key);
      map_insert(&map, key, &tasks[i]);
   }

   while(true) {
      sleep(1000);
      if(!monitor_open(m_dialog, w, tid))
         break;

      t = get_tasks(NULL, 0);
      api_task_t *new_tasks = malloc(sizeof(api_task_t)*t);
      int new_read = min(t, get_tasks(new_tasks, t));
      clear_w(w);
      for(int i = 0; i < new_read; i++) {
         if(!new_tasks[i].enabled) continue;

         char key[9];
         get_key(new_tasks[i].uid, key);

         api_task_t *old = map_lookup(&map, key);
         char buf[54];
         buf[52] = '\n';
         buf[53] = '\0';
         memset(buf, ' ', 52);
         char buf1[4];
         if(old)
            strcpy(buf1, "Old");
         else
            strcpy(buf1, "New");

         char buf2[32];
         char *name = strlen(new_tasks[i].main_window_name) > 0 ? new_tasks[i].main_window_name : new_tasks[i].exe_path;
         snprintf(buf2, 31, "%s task: %u %s", buf1, new_tasks[i].uid, name);

         char buf3[16];
         uint32_t ticks = old ? new_tasks[i].ticks - old->ticks : new_tasks[i].ticks;
         snprintf(buf3, 15, "ticks: %u", ticks);

         memcpy(buf, buf2, strlen(buf2));
         memcpy(buf + 32, buf3, strlen(buf3));

         write_str_w(buf, w);
      }

      free(tasks);
      tasks = new_tasks;
      map_free(&map);
      read = new_read;
      map_init(&map, read*2);

      for(int i = 0; i < read; i++) {
         if(!tasks[i].enabled) continue;
         char key[9];
         get_key(tasks[i].uid, key);
         map_insert(&map, key, &tasks[i]);
      }
   }

   free(tasks);
   map_free(&map);
   _exit(0);
}

void keypress(uint16_t c, int w) {
   if(dialog->ctrl_held) {
      if(c == 'r')
         refresh_callback(NULL, -1);
      if(c == 'm')
         create_thread(&monitor_thread);
   }
   dialog_keypress(c, w);
   end_subroutine();
}

void _start() {
   int index = get_free_dialog();
   dialog = get_dialog(index);
   if(!dialog_init(dialog, -1))
      exit(1);
   dialog_set_title(dialog, "Task Manager");
   int width = get_width();

   // tasks groupbox
   wo_t *groupbox = create_groupbox(5, 5, width - 10, 100, "Tasks");
   wo_t *menu = create_menu(2, 2, 0, 0);
   canvas_item_fill(groupbox_get_canvas(groupbox), menu);
   groupbox_add(groupbox, menu);
   ui_add(dialog->ui, groupbox);
   dialog_add(dialog, "tasks_group", groupbox);
   dialog_add(dialog, "tasks_menu", menu);

   // info groupbox
   groupbox = create_groupbox(5, 110, width - 10, 100, "Info");
   ui_add(dialog->ui, groupbox);
   dialog_add(dialog, "info_group", groupbox);

   wo_t *label = create_label(2, 2, width - 20, 20, "No task selected"); // none selected label
   ((label_t*)label->data)->bordered = false;
   groupbox_add(groupbox, label);
   dialog_add(dialog, "info_label", label);

   wo_t *canvas = create_canvas(2, 2, 0, 0); // canvas containing grid with task info
   canvas_item_fill(groupbox_get_canvas(groupbox), canvas);
   canvas->visible = false;
   get_canvas(canvas)->bordered = false;
   groupbox_add(groupbox, canvas);
   dialog_add(dialog, "info_canvas", canvas);

   wo_t *grid = create_grid(2, 2, 0, 0, 2, 2);
   canvas_item_fill(canvas, grid);
   dialog_add(dialog, "info_grid", label);
   label = create_label(2, 2, 0, 0, "");
   grid_item_fill_cell(grid, label);
   ((label_t*)label->data)->halign = false;
   ((label_t*)label->data)->valign = true;
   ((label_t*)label->data)->padding_left = 4;
   ((label_t*)label->data)->bordered = false;
   grid_add(grid, label, 0, 0);
   dialog_add(dialog, "info_label_id", label);
   canvas_add(canvas, grid);
   label = create_label(2, 2, 0, 0, "");
   grid_item_fill_cell(grid, label);
   ((label_t*)label->data)->halign = false;
   ((label_t*)label->data)->valign = true;
   ((label_t*)label->data)->padding_left = 4;
   ((label_t*)label->data)->bordered = false;
   grid_add(grid, label, 1, 0);
   dialog_add(dialog, "info_label_window", label);
   wo_t *button = create_button(10, 10, 0, 0, "End task");
   set_button_release(button, &end_task_callback);
   grid_item_fill_cell(grid, button);
   grid_add(grid, button, 0, 1);
   label = create_label(2, 2, 0, 0, "");
   grid_item_fill_cell(grid, label);
   ((label_t*)label->data)->halign = false;
   ((label_t*)label->data)->valign = true;
   ((label_t*)label->data)->padding_left = 4;
   ((label_t*)label->data)->bordered = false;
   grid_add(grid, label, 1, 1);
   dialog_add(dialog, "info_label_memory", label);

   int btns_width = (100 + 5) * 3 - 5;
   int offset = (width - btns_width) / 2;

   // launch tasks btn
   button = create_button(offset, 220, 100, 20, "Launch task");
   set_button_release(button, &launch_task_callback);
   ui_add(dialog->ui, button);
   // refresh btn
   button = create_button(offset + 105, 220, 100, 20, "Refresh");
   set_button_release(button, &refresh_callback);
   ui_add(dialog->ui, button);
   // close btn
   button = create_button(offset + 210, 220, 100, 20, "Close");
   set_button_release(button, &close_callback);
   ui_add(dialog->ui, button);

   show_tasks();
   clear();
   ui_redraw(dialog->ui);

   wm_override(WM_KEYPRESS, &keypress, -1);

   while(true) {
      snooze();
   }

}