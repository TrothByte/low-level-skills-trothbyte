#include <stdio.h>

int main(void)
{
    volatile int acc = 0;
    for (int i = 0; i < 1000000; ++i) {
        acc += i * i;
    }
    printf("%d\n", acc);
    return 0;
}
