/*
 * htonll_usage.c — DEMONSTRATION OF A NON-PORTABLE INVENTED HELPER.
 *
 * htonll / ntohll are NOT standard C, NOT POSIX, and do NOT exist on
 * Windows/MSVC. The portable POSIX/Linux names are htobe64/be64toh
 * (in <endian.h>); Windows has no direct equivalent. Correct approach:
 * write the shift-based serializer in examples/good/portable_serialize.c
 * and extend it with a put_u64_be using the same pattern.
 *
 * On this host the preprocessor stops with the #error below, which is the
 * demonstration: the name cannot be used portably. (On a system without
 * the #error, the undeclared identifier 'htonll' would be the failure.)
 *
 * Compile: gcc -Wall -Wextra -Werror htonll_usage.c   -> #error on Windows
 */
#include <stdint.h>

#ifdef _WIN32
#error "htonll/ntohll do not exist on Windows; use shifts or POSIX htobe64/be64toh"
#endif

int main(void) {
    uint64_t x = 0x1122334455667788ull;
    uint64_t be = htonll(x);   /* undeclared identifier outside POSIX */
    (void)be;
    return 0;
}
