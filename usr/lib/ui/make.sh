export GCC="i686-elf-gcc"
FLAGS="-ffreestanding -nostartfiles -nostdlib -mgeneral-regs-only -Wall -Wextra"

$GCC -O2 $FLAGS -c usr/lib/ui/ui_mgr.c -o o/lib/ui_mgr.o
$GCC -O2 $FLAGS -c usr/lib/ui/wo.c -o o/lib/wo.o
$GCC -O2 $FLAGS -c usr/lib/ui/ui_button.c -o o/lib/ui_button.o
$GCC -O2 $FLAGS -c usr/lib/ui/ui_label.c -o o/lib/ui_label.o
$GCC -O2 $FLAGS -c usr/lib/ui/ui_input.c -o o/lib/ui_input.o
$GCC -O2 $FLAGS -c usr/lib/ui/ui_menu.c -o o/lib/ui_menu.o
$GCC -O2 $FLAGS -c usr/lib/ui/ui_canvas.c -o o/lib/ui_canvas.o
$GCC -O2 $FLAGS -c usr/lib/ui/ui_grid.c -o o/lib/ui_grid.o
$GCC -O2 $FLAGS -c usr/lib/ui/ui_groupbox.c -o o/lib/ui_groupbox.o
$GCC -O2 $FLAGS -c usr/lib/ui/ui_image.c -o o/lib/ui_image.o
$GCC -O2 $FLAGS -c usr/lib/ui/ui_checkbox.c -o o/lib/ui_checkbox.o
$GCC -O2 $FLAGS -c usr/lib/ui/ui_textarea.c -o o/lib/ui_textarea.o
