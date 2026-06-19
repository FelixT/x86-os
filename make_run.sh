sudo qemu-system-i386 -drive file=hd.bin,format=raw,index=0,media=disk -monitor stdio -netdev vmnet-shared,id=n0,subnet-mask=255.255.255.0,start-address=10.5.0.1,end-address=10.5.0.5 -device rtl8139,netdev=n0 -net user -d guest_errors
#qemu-system-i386 -drive file=hd.bin,format=raw,index=0,media=disk -monitor stdio -net nic,model=rtl8139 -net user -d guest_errors
