/*
 * pathleak.c — demonstrates the build-path leak.
 *
 * __FILE__ expands to the path as given on the compile command line. Built
 * from two different absolute directories without -ffile-prefix-map, the
 * binaries embed different strings and differ; with
 *   -ffile-prefix-map=<absolute-dir>=src
 * the embedded path is normalized and the binaries match.
 */
#include <stdio.h>

int main(void) {
    puts(__FILE__);
    return 0;
}
