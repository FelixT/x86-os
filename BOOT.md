# Hardware boot

First 64k of the boot device (HDD, USB) needs to contain bootloader0|bootloader1.
First 512 bytes is stage 0 bootloader, automatically read into 0x7c00 and executed by BIOS.
This reads the remaining 63.5k (bootloader1) into 0x7e00 via BIOS interrupts and jumps to it.

Bootloader1 (cboot.c) reads the kernel from HDD via ATA from location 64000 (within partition) into 0x1000000.
Once booted, the system reads from a FAT filesystem located at 512000 (within partition) on the HDD.

### How to set this up to work on a USB

Copy first 64KiB of hd.bin to USB - note: some BIOS won't accept empty partition table - add dummy entry to USB copy only

Create usb.img - contains first 64KiB of hd.bin:

```
dd if=/dev/zero of=usb.img bs=1048576 count=1
dd if=hd.bin of=usb.img bs=65536 count=1 conv=notrunc
```

Copy to USB stick

```
lsblk #find the usb
sudo umount /dev/sdX?*
sudo dd if=usb.img of=/dev/sdX bs=65536 conv=fsync
```

BIOS: enable legacy mode, SATA mode set to compatibility

Set USB as first boot device

Create HDD partition (GPT and MBR supported) - note: must be within first 128 GiB as the ATA driver uses LBA28. MBR uses partition type 0x7F to identify, GPT uses typecode UUID BE59EB2E-91C0-4F56-8FEF-119A41DA9FAA.

(Assuming new partition is /dev/sda8, using GPT)

Set typecode so f3sys can identify it

```
sudo apt install gdisk
sudo sgdisk --typecode=<partition>:BE59EB2E-91C0-4F56-8FEF-119A41DA9FAA /dev/sda
sudo sgdisk --info=8 /dev/sda #check guid & last sector
```

Copy system (hd.bin) into partition

```
sudo dd if=hd.bin of=/dev/sda8 bs=4M conv=fsync status=progress
```

### Notes

Debian commands for testing partition/USB boot

```
truncate -s 64M hdd.img
sgdisk --new=1:2048:+8M --typecode=1:8300 --change-name=1:decoy hdd.img
sgdisk --new=2:0:+32M --typecode=2:BE59EB2E-91C0-4F56-8FEF-119A41DA9FAA --change-name=2:f3sys hdd.img
sgdisk --info=2 hdd.img #find "First sector"
dd if=hd.bin of=hdd.img bs=512 seek=<first sector> conv=notrunc
```

Mac commands for testing partition/USB boot

```
brew install gptfdisk
dd if=/dev/zero of=usb.img bs=1048576 count=1
dd if=hd.bin of=usb.img bs=65536 count=1 conv=notrunc
mkfile -n 64m hdd.img
sgdisk --new=1:2048:+8M --typecode=1:8300 --change-name=1:decoy hdd.img
sgdisk --new=2:0:+32M --typecode=2:BE59EB2E-91C0-4F56-8FEF-119A41DA9FAA --change-name=2:f3sys hdd.img
sgdisk --info=2 hdd.img #find "First sector"
dd if=hd.bin of=hdd.img bs=512 seek=<first sector> conv=notrunc
```

Boot in qemu

```
qemu-system-i386 -m 256 -usb \
-drive if=none,id=stick,file=usb.img,format=raw \
-device usb-storage,drive=stick,bootindex=0 \
-drive if=none,id=hd,file=hdd.img,format=raw \
-device ide-hd,drive=hd,bus=ide.0,unit=0,bootindex=1 \
-monitor stdio
```
