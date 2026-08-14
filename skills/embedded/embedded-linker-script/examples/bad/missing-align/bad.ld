ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 0x00100000
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00040000
}

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
        __data_start = .;
        *(.data*)
        __data_end = .;
    } > RAM AT> FLASH

    .bss :
    {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    __heap_start = .;
}
