#include <stdio.h>

const char hello[] = "hello, elf world";
const int table[4] = {1, 2, 3, 4};

int initialized = 41;
static int secret = 7;
int uninitialized;

int compute(int x)
{
    return x * 2;
}

static int hidden_helper(int x)
{
    return x + 1;
}

int main(void)
{
    uninitialized += compute(initialized);
    printf("%s %d %d\n", hello, uninitialized, hidden_helper(secret));
    return 0;
}
