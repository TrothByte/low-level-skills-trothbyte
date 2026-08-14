#include "libgeom.h"

static int calls;

int geom_area(int w, int h)
{
    calls++;
    return w * h;
}
