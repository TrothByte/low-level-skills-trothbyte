/*
 * fixed_timestamp.c — reproducible build timestamp.
 *
 * Uses SOURCE_DATE_EPOCH (via macro) instead of __DATE__/__TIME__, so the
 * compiled-in string is the same on every build.
 *
 *   gcc -DSOURCE_DATE_EPOCH=$(git log -1 --format=%ct) fixed_timestamp.c
 *   gcc -DSOURCE_DATE_EPOCH=1600000000 fixed_timestamp.c
 */
#include <stdio.h>
#include <time.h>

#ifndef SOURCE_DATE_EPOCH
#error "SOURCE_DATE_EPOCH must be defined (e.g. -DSOURCE_DATE_EPOCH=$(git log -1 --format=%ct))"
#endif

static const char *build_time(void) {
    static char buf[64];
    time_t t = (time_t)SOURCE_DATE_EPOCH;
    struct tm *tm = gmtime(&t);
    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S UTC", tm);
    return buf;
}

int main(void) {
    printf("build time: %s\n", build_time());
    return 0;
}
