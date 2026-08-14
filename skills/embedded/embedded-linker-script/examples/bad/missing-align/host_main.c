extern char __data_end;
extern char __bss_start;
extern char __bss_end;
extern char __heap_start;

volatile unsigned int sink;

char blob[2] = { 1, 2 };
char bss_blob[8];

void host_entry(void)
{
    sink = (unsigned int)(unsigned long)&__data_end;
    sink += (unsigned int)(unsigned long)&__bss_start;
    sink += (unsigned int)(unsigned long)&__bss_end;
    sink += (unsigned int)(unsigned long)&__heap_start;
}
