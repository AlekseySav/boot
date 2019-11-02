OUTPUT_FORMAT(elf32-i386)
SECTIONS
{
    . = 0;
    .text : { *(.text) }
    .data : { *(.data) }
    .bss : { *(.bss) }
}
