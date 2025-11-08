#!/bin/bash

export GCC="i686-elf-gcc"
export FLAGS="-ffreestanding -nostartfiles -nostdlib -mgeneral-regs-only -Wall -Wextra"

# userland programs

# asm raw binaries
nasm usr/prog1.asm -f bin -o o/prog1.bin
nasm usr/prog2.asm -f bin -o o/prog2.bin
nasm usr/progidle.asm -f bin -o o/progidle.bin

# elf binaries

# c libs
$GCC -c usr/lib/wo_api.c $FLAGS -o o/lib/wo_api.o
$GCC -c usr/lib/stdio.c $FLAGS -o o/lib/stdio.o
$GCC -c usr/lib/map.c $FLAGS -o o/lib/map.o
$GCC -c usr/lib/sort.c $FLAGS -o o/lib/sort.o
$GCC -c usr/lib/dialogs.c $FLAGS -o o/lib/dialogs.o
$GCC -c usr/lib/draw.c $FLAGS -o o/lib/drawusr.o

bash usr/lib/ui/make.sh

# c progs
$GCC $FLAGS usr/prog3.c -o o/prog3.elf 
$GCC $FLAGS usr/prog4.c -o o/prog4.elf o/lib/string.o o/lib/stdio.o
$GCC $FLAGS usr/files.c -o o/files.elf o/lib/string.o o/lib/wo_api.o o/lib/stdio.o o/lib/sort.o o/lib/dialogs.o
$GCC $FLAGS usr/bmpview.c -o o/bmpview.elf o/lib/wo_api.o o/lib/string.o o/lib/stdio.o o/lib/dialogs.o
$GCC $FLAGS usr/text.c -o o/text.elf o/lib/string.o o/lib/wo_api.o o/lib/stdio.o o/lib/dialogs.o
$GCC $FLAGS usr/term.c -o o/term.elf o/lib/string.o o/lib/stdio.o o/lib/sort.o
$GCC $FLAGS usr/calc.c -o o/calc.elf o/lib/string.o
$GCC $FLAGS usr/interp.c -o o/interp.elf o/lib/wo_api.o o/lib/string.o o/lib/stdio.o o/lib/map.o
$GCC $FLAGS usr/prog5.c -o o/prog5.elf o/lib/wo_api.o o/lib/string.o o/lib/stdio.o o/lib/map.o o/lib/dialogs.o

$GCC $FLAGS usr/apps.c -o o/apps.elf \
    o/lib/string.o \
    o/lib/stdio.o \
    o/lib/sort.o \
    o/lib/drawusr.o \
    o/lib/ui_mgr.o \
    o/lib/wo.o \
    o/lib/ui_button.o \
    o/lib/ui_label.o

$GCC $FLAGS usr/prog6.c -o o/prog6.elf \
    o/lib/string.o \
    o/lib/stdio.o \
    o/lib/sort.o \
    o/lib/drawusr.o \
    o/lib/ui_mgr.o \
    o/lib/wo.o \
    o/lib/ui_button.o \
    o/lib/ui_label.o

# copy programs to fs
cp o/prog1.bin fs_root/sys/prog1.bin
cp o/prog2.bin fs_root/sys/prog2.bin
cp o/progidle.bin fs_root/sys/progidle.bin
cp o/prog3.elf fs_root/sys/prog3.elf
cp o/prog4.elf fs_root/sys/prog4.elf
cp o/files.elf fs_root/sys/files.elf
cp o/bmpview.elf fs_root/sys/bmpview.elf
cp o/text.elf fs_root/sys/text.elf
cp o/term.elf fs_root/sys/term.elf
cp o/calc.elf fs_root/sys/calc.elf
cp o/apps.elf fs_root/sys/apps.elf
cp o/interp.elf fs_root/sys/interp.elf
cp o/prog5.elf fs_root/sys/prog5.elf
cp o/prog6.elf fs_root/sys/prog6.elf
