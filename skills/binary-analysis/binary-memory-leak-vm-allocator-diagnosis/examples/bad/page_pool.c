// intentionally incorrect — a page pool that reuses blocks but never returns
// them to the OS. Mirrors the Ghostty PageList.zig mechanism: "non-standard"
// pages are mapped directly, slot metadata is reset on reuse, and the unmap
// path (munmap on Linux / VirtualFree here) is skipped forever. Commit
// charge grows monotonically with the pool — the VM leak the diagnosis must
// find. This version runs on Windows (VirtualAlloc analog) so the mechanism
// is demonstrable on this host; the Linux/munmap and valgrind commands are in
// the evals README.
#include <stdio.h>
#include <windows.h>
#include <psapi.h>

#define BLOCK_SIZE (1u << 20)   /* 1 MiB block, 256 pages */
#define POOL_CAP    32          /* pool sized for "standard" page requests */
#define EXTRA       40          /* "non-standard" requests that bypass the pool */

static char *pool[POOL_CAP];
static int pool_used = 0;

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

/* A page pool that reuses blocks but never returns them to the OS. When a
 * "non-standard" request arrives, the slot metadata is reset but the block
 * (and its commit charge) stays mapped forever — the unmap path is skipped. */
static char *pool_get(void)
{
    if (pool_used < POOL_CAP) {
        char *p = (char *)VirtualAlloc(NULL, BLOCK_SIZE,
                                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!p) {
            return NULL;
        }
        pool[pool_used++] = p;
        return p;
    }
    /* pool exhausted: reuse a slot but never unmap the previous mapping */
    char *p = pool[pool_used - 1];
    return p;
}

static char *nonstandard_alloc(void)
{
    return (char *)VirtualAlloc(NULL, BLOCK_SIZE,
                                MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

int main(void)
{
    size_t before = commit_now();
    for (int i = 0; i < POOL_CAP; i++) {
        char *p = pool_get();
        if (!p) {
            return 1;
        }
        for (size_t off = 0; off < BLOCK_SIZE; off += 4096) {
            p[off] = (char)i;
        }
    }
    size_t after_pool = commit_now();

    for (int i = 0; i < EXTRA; i++) {
        char *p = nonstandard_alloc();   /* direct mapping, kept forever */
        if (!p) {
            return 1;
        }
        for (size_t off = 0; off < BLOCK_SIZE; off += 4096) {
            p[off] = (char)(i + 1);
        }
    }
    size_t after_extra = commit_now();

    printf("page_pool: before=%zu MiB after_pool=%zu MiB after_extra=%zu MiB "
           "pool_slots=%d\n",
           before >> 20, after_pool >> 20, after_extra >> 20, pool_used);
    return 0;
}
