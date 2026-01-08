// task manager

#include "lib/dialogs.h"
#include "prog.h"
#include "lib/stdio.h"
#include "../lib/string.h"

tasks_t tasks;
dialog_t *dialog;

int selected_task = -1;

void task_show(int index) {
   selected_task = index;
   api_task_t *task = &tasks.tasks[index];
   dialog_get(dialog, "info_label")->visible = false;
   dialog_get(dialog, "info_canvas")->visible = true;

   label_t *label_id = dialog_get(dialog, "info_label_id")->data;
   char buffer[64];
   sprintf(buffer, "Task ID:\n%i", task->id);
   strcpy(label_id->label, buffer);

   label_t *label_window = dialog_get(dialog, "info_label_window")->data;
   if(task->id == task->parentid) { // main thread
      sprintf(buffer, "Main window:\n%s", strlen(task->main_window_name)==0 ? "none" : task->main_window_name);
   } else { // child
      sprintf(buffer, "Task is child of task %i", task->parentid);
   }
   strcpy(label_window->label, buffer);

   ui_draw(dialog->ui);
}

void task_click(wo_t *wo, int index, int window) {
   (void)window;
   menu_t *menu = wo->data;
   int taskid = strtoint(menu->items[index].text+strlen("Task "));
   task_show(taskid);
}

void show_tasks() {
   tasks = get_tasks();
   wo_t *menu_wo = dialog_get(dialog, "tasks_menu");
   menu_t *menu = menu_wo->data;
   menu->item_count = 0;
   for(int i = 0; i < tasks.size; i++) {
      api_task_t *task = &tasks.tasks[i];
      if(!task->enabled) continue;
      char buffer[64];
      sprintf(buffer, "Task %i ", task->id);
      if(task->parentid == task->id) {
         strcat(buffer, task->main_window_name);
      } else {
         char buf2[16];
         sprintf(buf2, "(parent %i)", task->parentid);
         strcat(buffer, buf2);
      }
      add_menu_item(menu_wo, buffer, &task_click);
   }
   clear();
   ui_draw(dialog->ui);
}

void launch_task_return (char *out, int window) {
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Can't find window %i", window);
      return;
   }
   dialog->active = false;
   close_window(window);
   // have to do this before calling launch task as it ends subroutine
   launch_task(out, 0, NULL, false);
}

void launch_task_callback(wo_t *wo, int window) {
   (void)wo;
   (void)window;
   dialog_filepicker("/sys", &launch_task_return);
}

void end_task_callback(wo_t *wo, int window) {
   (void)wo;
   (void)window;
   end_task(selected_task);
   show_tasks();
}

void refresh_callback(wo_t *wo, int window) {
   (void)wo;
   (void)window;
   show_tasks();
}

void close_callback(wo_t *wo, int window) {
   (void)wo;
   close_window(window);
   exit(0);
}

void _start() {
   int index = get_free_dialog();
   dialog = get_dialog(index);
   dialog_init(dialog, -1);
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
   ((canvas_t*)canvas->data)->bordered = false;
   groupbox_add(groupbox, canvas);
   dialog_add(dialog, "info_canvas", canvas);

   wo_t *grid = create_grid(2, 2, 0, 0, 2, 2);
   canvas_item_fill(canvas, grid);
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

   // launch tasks btn
   button = create_button(5, 220, 100, 20, "Launch task");
   set_button_release(button, &launch_task_callback);
   ui_add(dialog->ui, button);
   // refresh btn
   button = create_button(110, 220, 100, 20, "Refresh");
   set_button_release(button, &refresh_callback);
   ui_add(dialog->ui, button);
   // close btn
   button = create_button(220, 220, 100, 20, "Close");
   set_button_release(button, &close_callback);
   ui_add(dialog->ui, button);

   show_tasks();

   while(true) {
      yield();
   }

}