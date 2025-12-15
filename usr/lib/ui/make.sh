export GCC="i686-elf-gcc"
FLAGS="-ffreestanding -nostartfiles -nostdlib -mgeneral-regs-only -Wall -Wextra"

$GCC $FLAGS -c usr/lib/ui/ui_mgr.c -o o/lib/ui_mgr.o
$GCC $FLAGS -c usr/lib/ui/wo.c -o o/lib/wo.o
$GCC $FLAGS -c usr/lib/ui/ui_button.c -o o/lib/ui_button.o
$GCC $FLAGS -c usr/lib/ui/ui_label.c -o o/lib/ui_label.o
$GCC $FLAGS -c usr/lib/ui/ui_input.c -o o/lib/ui_input.o
$GCC $FLAGS -c usr/lib/ui/ui_menu.c -o o/lib/ui_menu.o
$GCC $FLAGS -c usr/lib/ui/ui_canvas.c -o o/lib/ui_canvas.o