volatile unsigned int heartbeat;

int main(void)
{
    for (;;)
    {
        heartbeat++;
    }
}
