int shared_counter = 7;

int *counter_addr(void)
{
    return &shared_counter;
}
