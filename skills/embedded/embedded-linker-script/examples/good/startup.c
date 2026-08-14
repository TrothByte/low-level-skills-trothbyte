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

typedef void (*isr_t)(void);

/* CMSIS-style function-pointer vector table: valid on 32-bit targets and
   host-compilable (addresses are constant expressions for function pointers). */
__attribute__((section(".isr_vector")))
const isr_t vector_table[ISR_COUNT] = {
    [0] = (isr_t)0x20040000u,
    [1] = Reset_Handler,
};
