ENTRY(kmain)
OUTPUT_FORMAT(binary)

MEMORY {
    ram : org = 0x7e00, len = 63K
 }

SECTIONS {
    . = 0x7e00;
    boot_start = .;

    .text : {
        *(.text)
        *(.rodata)
    } >ram
    
    .data : {
        *(.data)
    } >ram

    .bss : {
        *(COMMON)
        *(.bss)
    } > ram

    boot_end = .;
     
}
