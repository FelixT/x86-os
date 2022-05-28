https://github.com/cirosantilli/x86-bare-metal-examples/tree/b4e4c124a3c3c329dcf09a5697237ed3b216a318#c-hello-world




objdump -b binary -D -m i8086 main.bin

objdump -b binary -D -m i386 o/main.bin

debug:
qemu-system-i386 -s -S -drive file=hd.bin,format=raw,index=0,media=disk -monitor stdio

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


sfdisk c.img

92d0 82cd