int *foo_addr(void);

int main(void)
{
    return *foo_addr() == 42 ? 0 : 1;
}
