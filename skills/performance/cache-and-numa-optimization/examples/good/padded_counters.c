/* GOOD: false sharing removed with padding.
 *
 * Each counter is a struct aligned to 64 bytes whose size is exactly one
 * cache line. Alignment fixes the base address; the size fixes the stride,
 * so counters[0] and counters[1] can never share a line. The worker code is
 * byte-for-byte identical to examples/bad/false_sharing.c -- only the layout
 * changed.
 *
 * Build and run:
 *   gcc -O2 -pthread padded_counters.c -o padded_counters.exe && ./padded_counters.exe
 */

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define ITERS 20000000u

struct padded_counter {
    _Alignas(64) volatile unsigned v;
}; /* _Alignas(64) on a member sets struct alignment AND sizeof to 64 -> one line per element */

static struct padded_counter counters[2];

static int pin_cpu[2];

static DWORD WINAPI worker(LPVOID arg) {
    unsigned idx = (unsigned)(uintptr_t)arg;
    unsigned v = 0;
    for (unsigned i = 0; i < ITERS; ++i) {
        v = counters[idx].v + 1;
        counters[idx].v = v;
    }
    return v;
}

static int pick_two_physical_cores(void) {
    DWORD len = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, NULL, &len);
    if (len == 0) return 0;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX info =
        (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)malloc(len);
    if (info == NULL) return 0;
    if (!GetLogicalProcessorInformationEx(RelationProcessorCore, info, &len)) {
        free(info);
        return 0;
    }
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX p = info;
    BYTE *end = (BYTE *)info + len;
    int found = 0;
    while ((BYTE *)p < end && found < 2) {
        if (p->Relationship == RelationProcessorCore &&
            p->Processor.GroupCount > 0) {
            DWORD_PTR mask = p->Processor.GroupMask[0].Mask;
            if (mask != 0) {
                unsigned cpu = (unsigned)__builtin_ctz((unsigned)mask);
                if (found == 0) {
                    pin_cpu[0] = (int)cpu;
                    found = 1;
                } else if (cpu != (unsigned)pin_cpu[0]) {
                    pin_cpu[1] = (int)cpu;
                    found = 2;
                }
            }
        }
        p = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)((BYTE *)p + p->Size);
    }
    free(info);
    return found == 2;
}

int main(void) {
    HANDLE t[2];
    DWORD tid[2];
    int pinned = pick_two_physical_cores();

    for (int i = 0; i < 2; ++i) {
        t[i] = CreateThread(NULL, 0, worker, (LPVOID)(uintptr_t)i, 0, &tid[i]);
        if (pinned) {
            SetThreadAffinityMask(t[i], (DWORD_PTR)1u << pin_cpu[i]);
        }
    }

    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    WaitForMultipleObjects(2, t, TRUE, INFINITE);
    QueryPerformanceCounter(&end);

    unsigned out[2];
    for (int i = 0; i < 2; ++i) {
        GetExitCodeThread(t[i], (LPDWORD)&out[i]);
        CloseHandle(t[i]);
    }

    printf("padded counters: {%u, %u}, pinned=%s, sizeof(counter)=%d\n",
           out[0], out[1], pinned ? "yes" : "no", (int)sizeof(struct padded_counter));
    printf("elapsed %.3f ms (%u iterations/thread)\n",
           1000.0 * (double)(end.QuadPart - start.QuadPart) / (double)freq.QuadPart,
           ITERS);
    return 0;
}
