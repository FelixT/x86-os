export CROSS="$HOME/opt/cross/bin"
export GCC="$CROSS/i686-elf-gcc"
export GAS="$CROSS/i686-elf-as"

nasm boot.asm -f bin -o boot.bin
qemu-system-i386 -hda boot.bin