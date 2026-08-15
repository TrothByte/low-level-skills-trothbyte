/*
 * GOOD: host-run demonstration of the "pin the version, check the export"
 * rule. On this host (Windows, no kernel headers) the kernel build is not
 * possible, so the rule is exercised with a stub resolver that models the
 * 6.9+ unexported-symbol outcome: the resolver returns NULL, and the good
 * path disables the feature loudly while the bad path would have no-oped.
 *
 * Build: gcc -Wall -Wextra -Werror -O2 availability_demo.c -o avail_demo
 * Run:   avail_demo [kernel_is_69_or_newer]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *stub_resolver(const char *name, int is_69_or_newer) {
    if (is_69_or_newer && strcmp(name, "sys_call_table") == 0)
        return NULL;              /* unexported since 6.9 */
    return (void *)0xFFFF800000000000ULL;
}

int main(int argc, char **argv) {
    int is69 = argc > 1 ? atoi(argv[1]) : 1;

    void *tbl = stub_resolver("sys_call_table", is69);
    if (!tbl) {
        printf("disabled: sys_call_table unavailable on this kernel\n");
        return 0;
    }
    printf("hook installed with verified table\n");
    return 0;
}
