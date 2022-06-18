export CROSS="$HOME/opt/cross/bin"
export GCC="$CROSS/i686-elf-gcc"
export GAS="$CROSS/i686-elf-as"
export LD="$CROSS/i686-elf-ld"

mkdir -p o

nasm boot.asm -f bin -o o/boot.bin

nasm main.asm -f elf32 -o o/main.o

nasm irq.asm -f elf32 -o o/irq.o


$GCC -c cmain.cpp -o o/cmain.o -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti -fno-common -mgeneral-regs-only -nostdlib -g -lgcc
$GCC -c font.c -o o/font.o -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-common -mgeneral-regs-only -nostdlib -lgcc
$GCC -c gui.c -o o/gui.o -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-common -mgeneral-regs-only -nostdlib -lgcc
$GCC -c terminal.c -o o/terminal.o -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-common -mgeneral-regs-only -nostdlib -lgcc
$GCC -c interrupts.c -o o/interrupts.o -ffreestanding -Wall -Wextra -fno-exceptions -fno-common -mgeneral-regs-only -nostdlib -lgcc
$LD -o o/main.bin -T linker.ld o/main.o o/cmain.o o/gui.o o/terminal.o o/irq.o o/interrupts.o o/font.o

cat o/boot.bin o/main.bin > hd.bin

# add programs at 28k 
nasm prog1.asm -f bin -o o/prog1.bin
nasm prog2.asm -f bin -o o/prog2.bin
nasm progidle.asm -f bin -o o/progidle.bin

dd if=/dev/zero of=hd2.bin bs=28000 count=1
dd if=./hd.bin of=hd2.bin bs=28000 count=1 conv=notrunc
cat hd2.bin o/progidle.bin o/prog1.bin o/prog2.bin > hd3.bin

qemu-system-i386 -drive file=hd3.bin,format=raw,index=0,media=disk -monitor stdio