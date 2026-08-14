int foo_add(int a, int b)
{
    return a * b;
}

int main(void)
{
    return foo_add(2, 3) == 5 ? 0 : 1;
}
