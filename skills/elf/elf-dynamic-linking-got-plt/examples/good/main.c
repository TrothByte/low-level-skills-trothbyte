#include "libcounter.h"
#include <stdio.h>

int main(void)
{
    counter_reset();
    counter_bump();
    counter_bump();
    counter_bump();
    printf("counter = %d\n", counter_get());
    return counter_get() == 3 ? 0 : 1;
}
