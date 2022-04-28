ENTRY(kmain)
OUTPUT_FORMAT(binary)

MEMORY {
     ram : org = 0x7e00, l = 32K
 }


SECTIONS {
    . = 0x7e00;

    .text : {
        *(.text)
        *(.rodata)
    } >ram

    .data : {
        *(.data)
    } >ram

    .bss : {
        *(.bss)
    } > ram
}
