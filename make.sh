export linux=1

export CROSS="$HOME/opt/cross/bin"
export GCC="$CROSS/i686-elf-gcc"
export GAS="$CROSS/i686-elf-as"
export LD="$CROSS/i686-elf-ld"

mkdir -p o
mkdir -p fs_root
mkdir -p fs_root/sys

nasm boot.asm -f bin -o o/boot.bin

nasm main.asm -f elf32 -o o/main.o

nasm irq.asm -f elf32 -o o/irq.o

$GCC -c cmain.cpp -o o/cmain.o -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti -fno-common -mgeneral-regs-only -nostdlib -g -lgcc
$GCC -c font.c -o o/font.o -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-common -mgeneral-regs-only -nostdlib -lgcc
$GCC -c gui.c -o o/gui.o -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-common -mgeneral-regs-only -nostdlib -lgcc
$GCC -c terminal.c -o o/terminal.o -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-common -mgeneral-regs-only -nostdlib -lgcc
$GCC -c interrupts.c -o o/interrupts.o -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-common -mgeneral-regs-only -nostdlib -lgcc
$GCC -c tasks.c -o o/tasks.o -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-common -mgeneral-regs-only -nostdlib -lgcc
$GCC -c ata.c -o o/ata.o -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-common -mgeneral-regs-only -nostdlib -lgcc
$GCC -c memory.c -o o/memory.o -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-common -mgeneral-regs-only -nostdlib -lgcc
$GCC -c fat.c -o o/fat.o -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-common -mgeneral-regs-only -nostdlib -lgcc
$GCC -c bmp.c -o o/bmp.o -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-common -mgeneral-regs-only -nostdlib -lgcc
$GCC -c elf.c -o o/elf.o -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-common -mgeneral-regs-only -nostdlib -lgcc
$GCC -c paging.c -o o/paging.o -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-common -mgeneral-regs-only -nostdlib -lgcc
$GCC -c window.c -o o/window.o -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-common -mgeneral-regs-only -nostdlib -lgcc
$LD -o o/main.bin -T linker.ld o/main.o o/cmain.o o/gui.o o/terminal.o o/irq.o o/interrupts.o o/tasks.o o/ata.o o/memory.o o/fat.o o/bmp.o o/elf.o o/paging.o o/window.o o/font.o

cat o/boot.bin o/main.bin > hd.bin

# userland programs
nasm prog1.asm -f bin -o o/prog1.bin
nasm prog2.asm -f bin -o o/prog2.bin
nasm progidle.asm -f bin -o o/progidle.bin
#nasm progidle.asm -f elf32 -o o/progidle.o
#$LD o/progidle.o -o o/progidle.elf
$GCC -ffreestanding -nostartfiles -nostdlib -mgeneral-regs-only -O2 -Wall -Wextra -Wl,--oformat=binary -c prog3.c -o o/prog3.bin 
$GCC -ffreestanding -nostartfiles -nostdlib -mgeneral-regs-only -Wall -Wextra prog3.c -o o/prog3.elf 

# copy programs to fs
cp o/prog1.bin fs_root/sys/prog1.bin
cp o/prog2.bin fs_root/sys/prog2.bin
cp o/prog3.bin fs_root/sys/prog3.bin
cp o/prog3.elf fs_root/sys/prog3.elf
cp o/progidle.bin fs_root/sys/progidle.bin
#cp o/progidle.elf fs_root/sys/progidle.elf

dd if=/dev/zero of=hd2.bin bs=64000 count=1
dd if=./hd.bin of=hd2.bin bs=64000 count=1 conv=notrunc
#cat hd2.bin o/progidle.bin o/prog1.bin o/prog2.bin > hd3.bin

# create FAT16 filesystem
# mkfs.fat from (brew install dosfstools)
rm -f fs.img

# if linux
if [ $linux==1 ]
then
   mkfs.fat -F 16 -n FATFS -C fs.img 12000
   sudo mkdir -p /mnt/fatfs
   sudo mount fs.img /mnt/fatfs
   sudo cp -R fs_root/* /mnt/fatfs
   sudo umount /mnt/fatfs
# if mac
else
   /usr/local/sbin/mkfs.fat -F 16 -n FATFS -C fs.img 12000
   # mount drive & copy files from fs_root dir
   hdiutil mount fs.img
   cp -R fs_root/ /Volumes/FATFS
   # unmount
   hdiutil unmount /Volumes/FATFS
fi

# add fs at 64000
cat hd2.bin fs.img > hd3.bin

   #qemu-system-i386 -s -S -drive file=hd3.bin,format=raw,index=0,media=disk -monitor stdio

   # gdb:
   # set disassembly-flavor intel
   #target remote localhost:1234 
   #continue
   #stepi
   #b
   #s

   qemu-system-i386 -drive file=hd3.bin,format=raw,index=0,media=disk -monitor stdio