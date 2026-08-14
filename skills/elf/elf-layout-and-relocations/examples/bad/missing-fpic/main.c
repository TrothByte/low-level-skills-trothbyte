#include <stdio.h>
#include "libnopic.h"

int main(void)
{
    printf("counter = %d\n", *counter_addr());
    return 0;
}
