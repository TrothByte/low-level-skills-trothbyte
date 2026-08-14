int qux_missing(int x);

int bar_caller(void)
{
    return qux_missing(1);
}
