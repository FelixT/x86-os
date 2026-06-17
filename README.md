## f3sys

custom 32-bit x86 OS project in C, NASM, C++

<img width="49%" src="https://felixt.github.io/img/OS7.png">

targets i686 (FPU not required)

### features

- Multitasking, per process threads
- FAT16 support, VFS
- GUI & Window manager
- Demand paging of process heaps
- ELF32 loader
- BMP viewer & editor, text editor, file manager, task manager, clock & shell
- f3basic script interpreter
- User mode ui/widget & dialog libraries
- Anonymous pipes
- Private futexes
- Shared memory
- PCI driver and privileged usermode network driver (RTL8139)

### build on mac/linux

Compile and run script:

`bash make.sh`

This command recompiles the kernel and usermode programs, creates hard drive image **hd.bin**, and runs this in QEMU.

Required packages (via brew / apt):

- i686-elf-gcc
- dosfstools
- qemu
- nasm
- mtools

### build on windows

Use WSL to build, then run with a windows QEMU binary

Graphics performance seems best using `-display gtk`

### hot reload usrmode/hd

`bash usr/make.sh && bash make_hd.sh && bash make_run.sh`

### architecture

#### boot

Stage 0 bootloader loads and jumps to stage 1 bootloader. VESA video mode is set, system is switched to 32 bit protected mode, kernel is loaded to 0x1000000 then jumped to.

Kernel init sequence (cmain) sets up core kernel subsystems and enables task switching.

#### memory

Task user stacks (identity mapped): 0x100000 – 0x110000

Kernel stack (identity mapped): 0x110000 – 0x180000

Kernel (identity mapped): 0x1000000 - 0x1040000 (256KB reserved)

Unified kernel/user heap: 0x1040000 – 0x3040000 (32MB)

Heap is mapped to a process's page directory via demand paging.

Shared memory vmapped to each process: 0xA0000000 - 0xB0000000

MMIO memory vmapped to each process: 0xB0000000 - 0xC0000000

#### kernel

f3sys implements a monolithic, interrupt driven graphical kernel.

interrupts.c includes handlers for timer, PS/2 keyboard, PS/2 mouse and software interrupts.

Preemptive multitasking utilises the timer, implementing a round robin scheduler as well as supporting a yield syscall.

Tasks are loaded from ELF32 binaries. I/O (e.g. keypress, mouse move) is processed by the window manager and surfaced to applications via callbacks on the main thread. These are queued if the application is already in a callback/subroutine, functioning as an event loop.

Tasks interact with kernel subsystems via syscalls, available calls are defined in api.h.

File I/O uses a kernel FAT16 driver which reads from an HDD using ATA PIO mode. User mode filesystem access is done via a VFS using UNIX style syscalls (open, read, write, close). When reads are performed, the thread is paused and woken upon completion. Reads are done in chunks to avoid blocking kernel processing.

#### usermode

Low level kernel calls are implemented in usr/prog.h.

Higher level libraries exist for file I/O (stdio.h) and malloc/free (stdlib.h).

A UI library provides widgets, along with a 'dialog' api for commonly used modals (message box, input dialog, file picker, colour picker).
