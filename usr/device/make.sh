# rtl drivers + ethernet linked into single net.elf device driver
$GCC $FLAGS usr/device/net/rtl8139.c usr/device/net/ethernet.c usr/device/net/ip.c -o o/device/net.elf o/lib/stdio.o o/lib/string.o o/lib/stdlib.o

cp o/device/net.elf fs_root/sys/device/net.elf
