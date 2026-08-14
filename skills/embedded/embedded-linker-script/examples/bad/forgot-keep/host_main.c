__attribute__((section(".isr_vector")))
const unsigned int fake_vectors[4] = {
    0x20040000u,
    1u, 2u, 3u,
};

volatile unsigned int sink;

void host_entry(void)
{
    sink = fake_vectors[0];
}
