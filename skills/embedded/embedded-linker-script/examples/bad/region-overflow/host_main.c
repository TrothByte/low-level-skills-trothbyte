char big[256];

volatile unsigned int sink;

void host_entry(void)
{
    sink = big[0];
}
