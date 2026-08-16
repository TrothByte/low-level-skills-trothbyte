/* BAD: a fuzz harness that (1) dereferences data[0] when size==0 (a
 * harness OOB that will be misreported as a target bug), (2) never resets
 * its parser state, so inputs contaminate each other, and (3) ignores the
 * result of the parse. Compiles cleanly; structurally wrong.
 * Compile: gcc -Wall -Wextra -Werror -O2 -c LLVMFuzzer_harness_bad.c
 * Marker: intentionally incorrect
 */
#include <stdint.h>
#include <string.h>

static uint32_t g_depth;   /* intentionally incorrect: global state that is
                              never reset between inputs */

static int parse_input(const uint8_t *data, size_t size)
{
    /* intentionally incorrect: no size guard; data[0] reads past the
       buffer when size == 0 */
    uint8_t first = data[0];
    for (size_t i = 0; i < size; i++) {
        if (data[i] == 0x10u) {
            g_depth++;           /* carries across inputs */
        }
    }
    return (int)first;           /* result discarded by caller */
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* intentionally incorrect: the entry point neither guards size nor
       resets g_depth; a crash here is a harness bug that will be reported
       as a target finding */
    (void)parse_input(data, size);
    return 0;
}
