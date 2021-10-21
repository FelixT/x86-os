export CROSS="$HOME/opt/cross/bin"
export GCC="$CROSS/i686-elf-gcc"
export GAS="$CROSS/i686-elf-as"

nasm boot.asm -f bin -o boot.bin
nasm main.asm -f bin -o main.bin

cat boot.bin main.bin > hd.bin

qemu-system-i386 -hda hd.bin