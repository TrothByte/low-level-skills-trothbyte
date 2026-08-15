/*
 * GOOD: host-verifiable mirror of the version-binding rule.
 * The critical out-of-tree property — vermagic must match the target kernel —
 * is enforced on any host by comparing the KDIR target string against the
 * deployment release. On Windows (this host) there is no /lib/modules, so this
 * compiles as a small self-check demonstrating the reasoning, while the real
 * build runs on Linux via the Makefile.
 */
#include <stdio.h>
#include <string.h>

static int check_vermagic_match(const char *built_against,
                                const char *deploy_on) {
    return strcmp(built_against, deploy_on) == 0;
}

int main(void) {
    const char *build_tree = "/lib/modules/6.6.0/build";
    const char *target = "/lib/modules/6.8.0/build";

    if (!check_vermagic_match(build_tree, target))
        printf("mismatch: module built against %s must not be loaded on %s\n",
               build_tree, target);
    return 0;
}
