#!/bin/bash

export GCC="i686-elf-gcc"
export GAS="i686-elf-as"
export LD="i686-elf-ld"
export OBJCOPY="i686-elf-objcopy"

c_files="font gui terminal interrupts events tasks ata memory fat bmp elf paging windowmgr window draw lib/string api windowobj window_term window_settings window_popup cboot fs"
o_files="o/start_32.o o/main.o o/cmain.o o/gui.o o/terminal.o o/irq.o o/interrupts.o o/events.o o/tasks.o o/ata.o o/memory.o o/fat.o o/bmp.o o/elf.o o/paging.o o/windowmgr.o o/window.o o/font.o o/draw.o o/lib/string.o o/api.o o/windowobj.o o/window_term.o o/window_settings.o o/window_popup.o o/fs.o"
boot_o_files="o/boot1.o o/memory.o o/ata.o o/cboot.o o/font.o o/draw.o o/terminal.o o/lib/string.o"

rm -r o/*

mkdir -p o
mkdir -p o/lib
mkdir -p fs_root
mkdir -p fs_root/sys

nasm boot/boot.asm -f bin -o o/boot.bin
nasm boot/boot1.asm -f elf32 -o o/boot1.o
nasm boot/main.asm -f elf32 -o o/main.o
nasm boot/start_32.asm -f elf32 -o o/start_32.o

nasm irq.asm -f elf32 -o o/irq.o


for file in $c_files; do
   $GCC -c $file.c -o o/$file.o -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-common -mgeneral-regs-only -nostdlib -lgcc
done

$GCC -c cmain.cpp -o o/cmain.o -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti -fno-common -mgeneral-regs-only -nostdlib -g -lgcc
$LD -o o/kernel.elf -T linker_kernel.ld $o_files 
$LD -o o/boot1.bin -T linker_boot.ld $boot_o_files

$OBJCOPY -O binary o/kernel.elf o/kernel.bin

# userland programs
nasm usr/prog1.asm -f bin -o o/prog1.bin
nasm usr/prog2.asm -f bin -o o/prog2.bin
nasm usr/progidle.asm -f bin -o o/progidle.bin
#nasm progidle.asm -f elf32 -o o/progidle.o
#$LD o/progidle.o -o o/progidle.elf

$GCC -c usr/lib/wo_api.c -ffreestanding -nostartfiles -nostdlib -mgeneral-regs-only -Wall -Wextra -o o/lib/wo_api.o
$GCC -c usr/lib/stdio.c -ffreestanding -nostartfiles -nostdlib -mgeneral-regs-only -Wall -Wextra -o o/lib/stdio.o

$GCC -ffreestanding -nostartfiles -nostdlib -mgeneral-regs-only -Wall -Wextra usr/prog3.c -o o/prog3.elf 
$GCC -ffreestanding -nostartfiles -nostdlib -mgeneral-regs-only -Wall -Wextra usr/prog4.c -o o/prog4.elf 
$GCC -ffreestanding -nostartfiles -nostdlib -mgeneral-regs-only -Wall -Wextra usr/files.c -o o/files.elf o/lib/string.o o/lib/wo_api.o  o/lib/stdio.o
$GCC -ffreestanding -nostartfiles -nostdlib -mgeneral-regs-only -Wall -Wextra usr/bmpview.c -o o/bmpview.elf o/lib/wo_api.o o/lib/string.o
$GCC -ffreestanding -nostartfiles -nostdlib -mgeneral-regs-only -Wall -Wextra usr/text.c -o o/text.elf o/lib/string.o o/lib/wo_api.o 
$GCC -ffreestanding -nostartfiles -nostdlib -mgeneral-regs-only -Wall -Wextra usr/term.c -o o/term.elf o/lib/string.o o/lib/stdio.o
$GCC -ffreestanding -nostartfiles -nostdlib -mgeneral-regs-only -Wall -Wextra usr/calc.c -o o/calc.elf o/lib/string.o

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
#cp o/progidle.elf fs_root/sys/progidle.elf

# fonts
nasm font.asm -f bin -o o/font11.bin
nasm font7.asm -f bin -o o/font7.bin
nasm font8.asm -f bin -o o/font8.bin
cp o/font11.bin fs_root/font/11.fon
cp o/font7.bin fs_root/font/7.fon
cp o/font8.bin fs_root/font/8.fon

# pad bootloader0|bootloader1 to 64k, write to hd2.bin
cat o/boot.bin o/boot1.bin > o/hd1.bin
dd if=/dev/zero of=o/hd2.bin bs=64000 count=1 status=none
dd if=o/hd1.bin of=o/hd2.bin bs=64000 count=1 conv=notrunc status=none

# add kernel at 64k, pad entire bootloader+kernel to 512k
#cat o/hd2.bin > o/main.bin
cat o/hd2.bin o/kernel.bin > o/main.bin
dd if=/dev/zero of=o/hd3.bin bs=512000 count=1 status=none
dd if=o/main.bin of=o/hd3.bin bs=512000 count=1 conv=notrunc status=none

# create FAT16 filesystem
# mkfs.fat from (brew install dosfstools)
rm -f fs.img

find . -name ".DS_Store" -delete
mkfs.fat -F 16 -n FATFS -C fs.img 10240
# mount drive & copy files from fs_root dir
hdiutil mount fs.img
cp -R fs_root/ /Volumes/FATFS
# unmount
hdiutil eject /Volumes/FATFS

# add fs at 512000
cat o/hd3.bin fs.img > hd.bin
chmod 644 hd.bin

if [[ $# -eq 0 ]]; then
   qemu-system-i386 -drive file=hd.bin,format=raw,index=0,media=disk -monitor stdio
else

   # Debug with LLDB

   #gdb-remote localhost:1234
   #w s e -- 0x123456
   osascript -e 'tell app "Terminal" to do script "echo gdb-remote localhost:1234;echo br s -a addr;lldb;"'

   qemu-system-i386 -s -S -drive file=hd.bin,format=raw,index=0,media=disk -monitor stdio

fi

# gdb:
# set disassembly-flavor intel
#target remote localhost:1234 
#continue
#stepi
#b
#s
