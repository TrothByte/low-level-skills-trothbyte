int foo_global = 42;

int *foo_addr(void)
{
    return &foo_global;
}
