#include <stdio.h>
#include "libfoo.h"

int main(void)
{
    printf("foo_add(2, 3) = %d\n", foo_add(2, 3));
    printf("calls = %d\n", foo_get_calls());
    return 0;
}
