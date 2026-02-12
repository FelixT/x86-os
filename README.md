## f3os

### build on mac/linux

Compile and run script:

`bash make.sh`

This command recompiles the kernel and usermode programs, creates hard drive image **hd.bin**, and runs this in qemu.

Required packages:

- i686-elf-gcc
- dosfstools
- qemu

### build on windows

Use WSL to build, then run with a windows QEMU binary

Graphics performance seems best using `-display gtk`
