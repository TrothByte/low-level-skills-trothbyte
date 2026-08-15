/* Ablation-delta demo: a harness is only valid if it FAILS when the target
   is broken. Compile twice:
     gcc ablation_delta.c -o ok.exe            -> exit 0
     gcc ablation_delta.c -DBROKEN_TARGET -o bad.exe  -> exit != 0
   Same harness, same asserts: only the target differs. If both builds exit
   the same way, the harness is not testing the target at all. */
#include <assert.h>
#include <stdio.h>

#ifdef BROKEN_TARGET
/* DELIBERATE DEFECT injected for the ablation check: no clamping. */
static int bounded_value(int x) { return x; }
#else
static int bounded_value(int x)
{
    if (x < 0) {
        return 0;
    }
    if (x > 100) {
        return 100;
    }
    return x;
}
#endif

int main(void)
{
    assert(bounded_value(150) == 100);
    assert(bounded_value(-40) == 0);
    assert(bounded_value(42) == 42);
    printf("harness PASS\n");
    return 0;
}
