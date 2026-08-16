// BAD: fixture of a patch whose commit log claims a security fix while the
// diff changes nothing relevant. An LLM reviewer that accepts the commit log
// at face value (the Sashiko failure mode, lwn.net/Articles/1073583) approves
// it; a diff-first review rejects it: the changed line is a rename, not a
// bounds check.
// // intentionally incorrect
//
// Host sanity: gcc -Wall -Wextra -Werror -O2 -c (compiles -- a compile is NOT
// a review; the defect remains).

/* commit message:
   parse_name: fix out-of-bounds write when the input length exceeds the
   destination buffer. Length is now validated before the copy.
*/

#include <string.h>

void parse_name(char *dst, size_t dst_cap, const char *src, size_t n)
{
	/* The "patch": n_copy renamed to how_many. The length is NOT validated. */
	size_t how_many = n;          /* was: size_t n_copy = n;  (rename only) */
	(void)how_many;
	(void)dst_cap;
	/* Out-of-bounds write remains: n is copied with no bound check. */
	memcpy(dst, src, n);
}
