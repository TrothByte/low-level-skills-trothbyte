ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 0x00100000
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00040000
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
    .isr_vector :
    {
        KEEP(*(.isr_vector))
    } > FLASH

    .text :
    {
        *(.text*)
        *(.rodata*)
    } > FLASH

    __etext = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        __data_start = .;
        *(.data*)
        __data_end = .;
    } > RAM AT> FLASH

    .bss :
    {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    . = ALIGN(8);
    __heap_start = .;
    . = ALIGN(8);
    __heap_end = ORIGIN(RAM) + LENGTH(RAM);
}
