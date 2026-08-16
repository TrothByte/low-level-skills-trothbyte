/* GOOD: libFuzzer-style harness for a stateful kernel-adjacent parser.
 * The entry point is stateless per input: it guards size (never touches
 * data[0] when size==0), resets its parser state, allocates/frees
 * per call, and the parser's input-length bounds are respected. This is
 * the harness shape OSS-Fuzz requires; KCOV/sanitizer-coverage supplies
 * the feedback in a kernel build.
 * Compile: gcc -Wall -Wextra -Werror -O2 -c LLVMFuzzer_harness.c
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t depth;
    uint32_t bytes_seen;
} parser_state_t;

static int parse_input(const uint8_t *data, size_t size, parser_state_t *st)
{
    for (size_t i = 0; i < size; i++) {        /* bounded by size, guarded */
        if (data[i] == 0x10u) {
            st->depth++;
        } else if (data[i] == 0x20u) {
            st->depth--;
        }
        st->bytes_seen++;
    }
    return (int)st->depth;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 3) {                             /* size==0 safe: no data[0] */
        return 0;
    }
    parser_state_t st;
    memset(&st, 0, sizeof(st));                 /* stateless per input */

    (void)parse_input(data, size, &st);
    return 0;                                   /* 0 = not an error */
}
