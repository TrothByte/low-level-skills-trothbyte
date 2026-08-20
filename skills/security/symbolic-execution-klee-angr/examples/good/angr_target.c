/* GOOD angr target: a binary with a reachable crash that depends on the
 * first four bytes of an input buffer. Two entry points serve two flows:
 *
 *   main()        — standalone/host flow: copies argv[1] into g_input and
 *                   calls analyze_me(); the magic prefix "VULN" crashes.
 *                   gcc -g -O0 angr_target.c -o angr_target.exe
 *                   angr_target.exe VULN   (crash, exit 0xC0000005)
 *
 *   analyze_me()  — angr flow (TARGET-ONLY, requires angr): the angr
 *                   script starts blank_state at analyze_me, stores a
 *                   symbolic 16-byte BVS into the g_input global, and
 *                   the explorer forks on each character comparison to
 *                   find the state reaching crash_me. This avoids the
 *                   MinGW CRT startup and argv plumbing that drag angr
 *                   into __tmainCRTStartup — the "model the environment
 *                   (globals + hooks), do not execute the CRT" rule.
 */
#include <string.h>

static char g_input[16];

static int crash_me(void)
{
    volatile int *p = 0;
    return *p;      /* deliberate NULL deref (crash) */
}

int analyze_me(void)
{
    if (g_input[0] == 'V' && g_input[1] == 'U' &&
        g_input[2] == 'L' && g_input[3] == 'N')
        return crash_me();
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 2) {
        size_t n = strlen(argv[1]);
        if (n > sizeof g_input)
            n = sizeof g_input;
        memcpy(g_input, argv[1], n);
    }
    return analyze_me();
}
