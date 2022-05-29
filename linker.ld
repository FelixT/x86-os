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
        *(COMMON)
        *(.bss)
    } > ram
    /* allocate stack */
    . = ALIGN( 8 );
    . = . + 0x00001000;  
    tos_kernel = .; /* currently unused... */

    . = . + 0x00001000;
    tos_program = .; /* program stacks */
}
