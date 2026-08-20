// GOOD: annotated Checked C — the pattern a migration should produce.
// TARGET-ONLY: requires the Checked C clang fork. Stock gcc DOES NOT accept
// checked pointer keywords and this file will NOT compile under gcc.
//
// Compile:  clang -fcheckedc-extension -Wall -Wextra -Werror -o annotated_checkedc annotated_checkedc.c
// Run:      ./annotated_checkedc                     (happy path, exit 0)
// OOB probe: clang -fcheckedc-extension -Wall -Wextra -Werror -DOOB_PROBE \
//              -o oob_probe annotated_checkedc.c && ./oob_probe
//   expect: runtime trap (bounds check fires) on the deliberate OOB write.
//
// Demonstrates: _Ptr (single object), _Array_ptr + count (counted array),
// _Nt_array_ptr (null-terminated array), _Checked block, _Dynamic_check.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// _Ptr<T>: non-null pointer to a single object. No arithmetic, no subscript.
static int deref_single(int _Ptr p) {
    return *p;
}

// _Array_ptr<T> : count(n): pointer to n contiguous elements.
// The bound parameter may be declared after the pointer in Checked C.
static long summarize(_Array_ptr<const int> a : count(n), size_t n) {
    long acc = 0;
    for (size_t i = 0; i < n; ++i) {
        acc += a[i];          // i in [0, n): provably in bounds
    }
    return acc;
}

// _Nt_array_ptr<T>: null-terminated array; bounds derived from strlen+1.
static size_t nt_len(_Nt_array_ptr<const char> s) {
    return strlen((const char *)s);
}

int main(void) {
    int x = 42;
    int _Ptr px = &x;                               // single object
    printf("single=%d\n", deref_single(px));

    int arr[4] = {1, 2, 3, 4};
    _Array_ptr<int> pa : count(4) = arr;            // counted array
    printf("sum=%ld\n", summarize(pa, 4));

    _Nt_array_ptr<char> s = "hello";                // literal is provably terminated
    printf("len=%zu\n", nt_len(s));

    // _Checked block: every pointer declared inside must be checked.
    _Checked {
        _Array_ptr<int> pb : count(4) = arr;
        _Dynamic_check(pb[0] == 1);                 // runtime assertion of a bound property
    }

#ifdef OOB_PROBE
    // The migration must prove the compiler actually enforces the bound:
    // pc declares count(2) but we write pc[3] — the inserted bounds check traps.
    _Array_ptr<int> pc : count(2) = arr;
    pc[3] = 0;
    printf("unreachable\n");
#endif

    return 0;
}
