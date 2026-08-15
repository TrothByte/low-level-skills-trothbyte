// GOOD: allocations are released to the OS (VirtualFree/MEM_RELEASE — the
// Windows analog of munmap) when the pool is done with them. Commit charge
// returns to baseline after the loop. This demonstrates the fix direction for
// the page-pool leak: the unmap path must actually run.
#include <stdio.h>
#include <windows.h>
#include <psapi.h>

#define BLOCK_SIZE (1u << 20)   /* 1 MiB block, 256 pages */

static size_t commit_now(void)
{
    PROCESS_MEMORY_COUNTERS_EX c;
    c.cb = sizeof(c);
    if (!GetProcessMemoryInfo(GetCurrentProcess(),
                              (PROCESS_MEMORY_COUNTERS *)&c, sizeof(c))) {
        return 0;
    }
    return (size_t)c.PagefileUsage;
}

int main(void)
{
    size_t before = commit_now();
    char *blocks[40];
    for (int i = 0; i < 40; i++) {
        char *p = (char *)VirtualAlloc(NULL, BLOCK_SIZE,
                                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!p) {
            return 1;
        }
        for (size_t off = 0; off < BLOCK_SIZE; off += 4096) {
            p[off] = (char)i;          /* touch every page so it is committed */
        }
        blocks[i] = p;
    }
    size_t during = commit_now();

    for (int i = 0; i < 40; i++) {
        VirtualFree(blocks[i], 0, MEM_RELEASE);   /* return pages to the OS */
    }
    size_t after = commit_now();

    printf("release_to_os: before=%zu MiB during=%zu MiB after=%zu MiB\n",
           before >> 20, during >> 20, after >> 20);
    return 0;
}
