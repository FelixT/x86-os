## Debugging kernel/userland progs

Work out which function/instructions is crashing:

objdump -d --start-address=0x804d000 --stop-address=0x804e000 <kernel/prog6/etc.elf>

## Compile user progs and boot

bash usr/make.sh && bash make_hd_and_run.sh

## Misc old notes

https://github.com/cirosantilli/x86-bare-metal-examples/tree/b4e4c124a3c3c329dcf09a5697237ed3b216a318#c-hello-world

objdump -b binary -D -m i8086 main.bin

objdump -b binary -D -m i386 o/main.bin

/usr/local/Cellar/binutils/2.37/bin/objdump -b binary -D -m i386 o/main.bin
~/opt/cross/bin/

debug:
qemu-system-i386 -s -S -drive file=hd4.bin,format=raw,index=0,media=disk -monitor stdio

qemu:
   info registers
   
gdb:
   target remote localhost:1234 
   continue
   stepi
   b
   s


pmemsave 0x00 100000 outfile.mem
HxD



dd if = ./hd.bin of = c.img bs = 512 count = 1 conv = notrunc

32768

dd if=./hd.bin of=c.img bs=65536 count=1 conv=notrunc

bochsrc: ata0-master: type=disk, path="c.img", mode=flat
dd if=./hd4.bin of=c.img bs=512 count=20000 conv=notrunc
cylinders=20, heads=16, spt=63

sfdisk c.img


https://rwmj.wordpress.com/2016/04/21/tip-poor-mans-qemu-breakpoint/