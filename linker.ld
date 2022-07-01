ENTRY(kmain)
OUTPUT_FORMAT(binary)

MEMORY {
    ram : org = 0x7e00, l = 64K
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

    /* allocate stacks */
    . = ALIGN( 8 );
    . = . + 0x00001000;  
    tos_kernel = .;

    . = . + 0x00001000;
    tos_program = .; /* program stacks */

    . = 0x00100000;
    heap_kernel = .;
    . = . + 0x0100000;
     
}
