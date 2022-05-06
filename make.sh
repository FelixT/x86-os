export CROSS="$HOME/opt/cross/bin"
export GCC="$CROSS/i686-elf-gcc"
export GAS="$CROSS/i686-elf-as"
export LD="$CROSS/i686-elf-ld"

nasm boot.asm -f bin -o boot.bin

nasm main.asm -f elf32 -o main.o

nasm irq.asm -f elf32 -o irq.o

$GCC -c cmain.cpp -o cmain.o -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti -fno-common
$GCC -c interrupts.c -o interrupts.o -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-common
$LD -o main.bin -T linker.ld main.o cmain.o irq.o interrupts.o 



cat boot.bin main.bin > hd.bin

qemu-system-i386 -drive file=hd.bin,format=raw,index=0,media=disk -monitor stdio