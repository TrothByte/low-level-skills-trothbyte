#include "greet.h"
#include <stdio.h>

static char buf[128];

const char *greet_name(const char *name)
{
    snprintf(buf, sizeof buf, "hello, %s", name);
    return buf;
}
