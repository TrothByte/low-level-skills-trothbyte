#include "libfoo.h"

static int calls;

int foo_add(int a, int b)
{
    calls++;
    return a + b;
}

int foo_get_calls(void)
{
    return calls;
}
