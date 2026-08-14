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
    } > RAM

    .text :
    {
        *(.text*)
        *(.rodata*)
    } > FLASH
}
