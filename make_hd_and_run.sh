
# pad bootloader0|bootloader1 to 64k, write to hd2.bin
cat o/boot.bin o/boot1.bin > o/hd1.bin
dd if=/dev/zero of=o/hd2.bin bs=64000 count=1 status=none
dd if=o/hd1.bin of=o/hd2.bin bs=64000 count=1 conv=notrunc status=none

# add kernel at 64k, pad entire bootloader+kernel to 512k
#cat o/hd2.bin > o/main.bin
cat o/hd2.bin o/kernel.bin > o/main.bin
dd if=/dev/zero of=o/hd3.bin bs=512000 count=1 status=none
dd if=o/main.bin of=o/hd3.bin bs=512000 count=1 conv=notrunc status=none

# create FAT16 filesystem
# mkfs.fat from (brew install dosfstools)
rm -f fs.img

find . -name ".DS_Store" -delete
mkfs.fat -F 16 -n FATFS -C fs.img 10240
# mount drive & copy files from fs_root dir with mcopy (faster than mount & unmount bottleneck)
mcopy -s -i fs.img fs_root/* ::

# add fs at 512000
cat o/hd3.bin fs.img > hd.bin
chmod 644 hd.bin

if [[ $# -eq 0 ]]; then
   qemu-system-i386 -drive file=hd.bin,format=raw,index=0,media=disk -monitor stdio
else

   # Debug with LLDB

   #gdb-remote localhost:1234
   #w s e -- 0x123456
   osascript -e 'tell app "Terminal" to do script "echo gdb-remote localhost:1234;echo br s -a addr;lldb;"'

   qemu-system-i386 -s -S -drive file=hd.bin,format=raw,index=0,media=disk -monitor stdio

fi
