#include "libcounter.h"

static int counter;

void counter_reset(void)
{
    counter = 0;
}

void counter_bump(void)
{
    counter++;
}

int counter_get(void)
{
    return counter;
}
