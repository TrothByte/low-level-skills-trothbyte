#include <stdio.h>

int compute(int x);

int helper(int x)
{
    return x * 100;
}

int main(void)
{
    int r = compute(1);
    printf("compute(1) = %d\n", r);
    return 0;
}
