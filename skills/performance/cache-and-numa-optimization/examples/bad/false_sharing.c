/* BAD: false sharing.
 *
 * Two threads each increment their OWN element of a shared two-int array.
 * The elements are adjacent, so they live in the same 64-byte cache line.
 * Every store by thread 0 invalidates the line in the cache of the core
 * running thread 1 and vice versa. The threads fight for line ownership
 * instead of running in parallel: each increment becomes a cross-core line
 * transfer, not a private L1 hit. There is no data race (each thread writes
 * a distinct memory location) -- the slowdown is pure coherency traffic.
 *
 * Build and run:
 *   gcc -O2 -pthread false_sharing.c -o false_sharing.exe && ./false_sharing.exe
 * Compare with examples/good/padded_counters.c, identical worker code with
 * only the counter layout changed.
 *
 * Why the affinity pinning: hyper-thread siblings share L1/L2, which hides
 * the coherency traffic. Pinning the two workers to different physical cores
 * keeps the benchmark honest on any machine.
 */

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define ITERS 20000000u

static volatile unsigned counters[2];

static int pin_cpu[2];

static DWORD WINAPI worker(LPVOID arg) {
    unsigned idx = (unsigned)(uintptr_t)arg;
    unsigned v = 0;
    for (unsigned i = 0; i < ITERS; ++i) {
        v = counters[idx] + 1;
        counters[idx] = v;
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

    printf("false sharing: counters {%u, %u}, pinned=%s\n",
           out[0], out[1], pinned ? "yes" : "no");
    printf("elapsed %.3f ms (%u iterations/thread)\n",
           1000.0 * (double)(end.QuadPart - start.QuadPart) / (double)freq.QuadPart,
           ITERS);
    return 0;
}
