qemu-system-i386 -drive file=hd.bin,format=raw,index=0,media=disk -monitor stdio -net nic,model=rtl8139 -net user -d guest_errors
