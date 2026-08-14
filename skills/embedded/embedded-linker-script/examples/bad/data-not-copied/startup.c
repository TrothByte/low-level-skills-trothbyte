extern char __etext;
extern char __data_start;
extern char __data_end;
extern char __bss_start;
extern char __bss_end;
extern int main(void);

void Reset_Handler(void)
{
    char *src = &__etext;
    char *dst = &__data_start;
    while (dst < &__data_end)
    {
        *dst++ = *src++;
    }
    dst = &__bss_start;
    while (dst < &__bss_end)
    {
        *dst++ = 0;
    }
    main();
    for (;;)
    {
    }
}

#define ISR_COUNT 128

__attribute__((section(".isr_vector")))
const unsigned int vector_table[ISR_COUNT] = {
    [0] = 0x20040000u,
    [1] = (unsigned int)&Reset_Handler,
};
