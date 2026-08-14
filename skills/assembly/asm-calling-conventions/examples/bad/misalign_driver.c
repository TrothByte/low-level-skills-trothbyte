// BAD-case driver for the stack-alignment fault. Run under Windows x64 (MinGW):
//   gcc misalign_driver.c misaligned_call.s ../good/win64_caller_aligned.s -o misalign
// Expected: aligned caller prints OK; misaligned caller faults (access violation).
#include <stdio.h>

double caller_aligned(void);
double caller_misaligned(void);

int main(void)
{
    double x = caller_aligned();
    printf("aligned caller OK, result %g\n", x);
    fflush(stdout);
    double y = caller_misaligned();
    printf("misaligned caller OK, result %g\n", y);
    return 0;
}
