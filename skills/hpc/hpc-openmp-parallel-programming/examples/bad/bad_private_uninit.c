// intentionally incorrect — BAD example: private variable read before write.
//
// `tmp` is private, which means it is UNINITIALIZED inside the region. Reading
// it before writing is undefined behavior (typically garbage). `firstprivate`
// would copy the outer value in instead.

#include <omp.h>
#include <stdio.h>

int main(void)
{
    int tmp = 0;                         // outer value: 0
    int bad = 0;

    #pragma omp parallel for
    for (int i = 0; i < 100; i++) {
        int local = tmp;                 // BUG: reads uninitialized private tmp
        bad += local;
    }

    printf("bad_private_uninit: bad=%d (should be 0 if tmp were firstprivate)\n",
           bad);
    return 0;
}
