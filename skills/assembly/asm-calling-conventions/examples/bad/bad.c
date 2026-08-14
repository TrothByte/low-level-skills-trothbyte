// BAD-case driver. Build and run under Windows x64 (MinGW GCC):
//   gcc bad.c sysv_args_on_windows.s missing_shadow_space.s misaligned_call.s -o bad
//   ./bad
// Expected: win_add -> 42, bad_sysv_add -> garbage (96), no-shadow caller -> 0x1
// (sentinel clobbered), aligned caller -> OK, misaligned caller -> access violation.
#include <stdio.h>

long long win_add(long long a, long long b);
long long bad_sysv_add(long long a, long long b);
long long caller_noshadow(void);
long long callee_writes_shadow(long long a, long long b);

int main(void)
{
    printf("win_add(40,2) = %lld\n", win_add(40, 2));
    printf("bad_sysv_add(40,2) = %lld\n", bad_sysv_add(40, 2));
    printf("no-shadow caller returned 0x%llx\n", caller_noshadow());
    printf("callee_writes_shadow(1,2) = %lld\n", callee_writes_shadow(1, 2));
    return 0;
}
