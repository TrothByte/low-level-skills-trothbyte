#include <stdio.h>
#include "libgeom.h"

int global_from_main = 100;

int main(void)
{
    printf("area = %d, global = %d\n", geom_area(3, 4), global_from_main);
    return 0;
}
