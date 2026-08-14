extern char __etext;
extern char __data_start;
extern char __data_end;
extern char __bss_start;
extern char __bss_end;

volatile unsigned int sink;

void host_entry(void)
{
    sink = (unsigned int)(unsigned long)&__etext;
    sink += (unsigned int)(unsigned long)&__data_start;
    sink += (unsigned int)(unsigned long)&__data_end;
    sink += (unsigned int)(unsigned long)&__bss_start;
    sink += (unsigned int)(unsigned long)&__bss_end;
}
