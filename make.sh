export CROSS="$HOME/opt/cross/bin"
export GCC="$CROSS/i686-elf-gcc"
export GAS="$CROSS/i686-elf-as"
export LD="$CROSS/i686-elf-ld"

mkdir -p o

nasm boot.asm -f bin -o o/boot.bin

nasm main.asm -f elf32 -o o/main.o

nasm irq.asm -f elf32 -o o/irq.o

$GCC -c cmain.cpp -o o/cmain.o -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti -fno-common
$GCC -c interrupts.c -o o/interrupts.o -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-common
$LD -o o/main.bin -T linker.ld o/main.o o/cmain.o o/irq.o o/interrupts.o 

cat o/boot.bin o/main.bin > hd.bin

qemu-system-i386 -drive file=hd.bin,format=raw,index=0,media=disk -monitor stdio