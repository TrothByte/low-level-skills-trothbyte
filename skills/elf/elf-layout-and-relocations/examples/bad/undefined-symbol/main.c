#include <stdio.h>

int never_defined_here(void);

int main(void)
{
    printf("result = %d\n", never_defined_here());
    return 0;
}
