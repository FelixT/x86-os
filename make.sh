export CROSS="$HOME/opt/cross/bin"
export GCC="$CROSS/i686-elf-gcc"
export GAS="$CROSS/i686-elf-as"
export LD="$CROSS/i686-elf-ld"

nasm boot.asm -f bin -o boot.bin

nasm main.asm -f elf32 -o main.o
$GCC -c cmain.cpp -o cmain.o -m16 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti
$LD -o main.bin -T linker.ld main.o cmain.o



cat boot.bin main.bin > hd.bin

qemu-system-i386 -drive file=hd.bin,format=raw,index=0,media=disk