ENTRY(start_32)
OUTPUT_FORMAT(elf32-i386)

MEMORY {
    ram (rwx) : ORIGIN = 0x06400000, LENGTH = 127K
}

SECTIONS {
    . = 0x06400000;

    .text BLOCK(4K) : ALIGN(4K) {
        *(.text)
        *(.rodata)
    } > ram

    .data  BLOCK(4K) : ALIGN(4K) {
        *(.data)
    } > ram

    .bss  BLOCK(4K) : ALIGN(4K) {
        *(COMMON)
        *(.bss)
    } > ram

    kernel_end = .;
}