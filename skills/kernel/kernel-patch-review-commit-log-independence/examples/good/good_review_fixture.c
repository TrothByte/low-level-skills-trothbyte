// GOOD: the fixed version of the fixture. The diff-first review accepts this
// patch because the changed line is the actual bound check dominating the
// copy. Host sanity: gcc -Wall -Wextra -Werror -O2 -c (exit 0, verified).

/* commit message:
   parse_name: fix out-of-bounds write when the input length exceeds the
   destination buffer. Length is now validated before the copy.
*/

#include <string.h>

void parse_name(char *dst, size_t dst_cap, const char *src, size_t n)
{
	/* The real fix: reject oversized lengths before the copy. */
	if (n > dst_cap)
		return;
	memcpy(dst, src, n);
}
