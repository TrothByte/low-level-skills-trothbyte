// intentionally incorrect — BAD example: atomic applied to a compound statement.
//
// `#pragma omp atomic` is defined only for a single update of one lvalue
// (`x op= expr`, `x++`, etc.). Wrapping a compound statement is NOT valid:
// on this machine gcc rejects it at compile time
//   "error: expected expression before '{' token"
// (exit 1). In a compiler that accepts it as a no-op, the read-modify-write
// would not be atomic and the count would be wrong. The correct construct for
// a compound update is `#pragma omp critical`.

#include <omp.h>
#include <stdio.h>

int main(void)
{
    int count = 0;

    #pragma omp parallel for
    for (int i = 0; i < 100000; i++) {
        #pragma omp atomic
        {
            int t = count;              // not a single-lvalue atomic update
            count = t + 1;
        }
    }

    printf("bad_atomic_section: count=%d (expected 100000)\n", count);
    return (count == 100000) ? 0 : 1;
}
