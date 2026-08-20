/*
 * nonrepro_time.c — intentionally non-reproducible.
 *
 * __DATE__ and __TIME__ are expanded at preprocessing time to the wall-clock
 * date/time of the compile. Two builds separated by one second embed
 * different strings and produce different binaries. This is the anti-pattern
 * that reproducible-builds-firmware removes.
 */
#include <stdio.h>

int main(void) {
    printf("build: %s %s\n", __DATE__, __TIME__);
    return 0;
}
