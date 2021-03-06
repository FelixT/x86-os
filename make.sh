export CROSS="$HOME/opt/cross/bin"
export GCC="$CROSS/i686-elf-gcc"
export GAS="$CROSS/i686-elf-as"
export LD="$CROSS/i686-elf-ld"

mkdir -p o
mkdir -p fs_root

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
$LD -o o/main.bin -T linker.ld o/main.o o/cmain.o o/gui.o o/terminal.o o/irq.o o/interrupts.o o/tasks.o o/ata.o o/memory.o o/fat.o o/bmp.o o/font.o

cat o/boot.bin o/main.bin > hd.bin

# add programs at 42k 
nasm prog1.asm -f bin -o o/prog1.bin
nasm prog2.asm -f bin -o o/prog2.bin
nasm progidle.asm -f bin -o o/progidle.bin

dd if=/dev/zero of=hd2.bin bs=42000 count=1
dd if=./hd.bin of=hd2.bin bs=42000 count=1 conv=notrunc
cat hd2.bin o/progidle.bin o/prog1.bin o/prog2.bin > hd3.bin

# create FAT32 filesystem
# mkfs.fat from (brew install dosfstools)
rm fs.img
mkfs.fat -F 16 -n FATFS -C fs.img 10000
# copy files from fs_root dir
hdiutil mount fs.img
cp -R fs_root/ /Volumes/FATFS
# unmount
hdiutil unmount /Volumes/FATFS

# add fs at 42000 + 512*3 (32536/0x7f18)
cat hd3.bin fs.img > hd4.bin

qemu-system-i386 -drive file=hd4.bin,format=raw,index=0,media=disk -monitor stdio