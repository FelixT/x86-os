
#include <stddef.h>
#include <stdbool.h>

#include "prog.h"
#include "lib/ui/ui_mgr.h"
#include "lib/ui/ui_label.h"
#include "lib/sort.h"
#include "lib/draw.h"

void _start() {
   set_window_size(160, 70);
   set_window_title("About");
   override_draw(NULL, -1);

   surface_t surface = get_surface();
   ui_mgr_t *ui = ui_init(&surface, -1);

   ui_add(ui, create_label(10, 10, 140, 40, "f3sys v0.3.3"));
   ui_draw(ui);

   while(true) {
      yield();
   }
}