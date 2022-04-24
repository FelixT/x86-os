ENTRY(kmain)
OUTPUT_FORMAT(binary)

MEMORY {
     ram : org = 0x7e00, l = 8K
 }


SECTIONS {
    . = 0x7e00;

    .text : {
        *(.text)
    } >ram
}
