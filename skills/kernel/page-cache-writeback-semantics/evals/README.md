# Evaluation — page-cache-writeback-semantics

Skill: `skills/kernel/page-cache-writeback-semantics`. Stability target:
`evaluated`.

## Verified facts (host, this run)

- All examples compile clean with `gcc -Wall -Wextra -Werror -O2` using
  self-contained stubs (`examples/stubs.h`) — no kernel headers required,
  no threads, fully deterministic.
- The good example asserts the full semantics: dirty accounting matches the
  page model, fsync/fdatasync durability matches the durable-store model,
  redirty race leaves the page dirty, background flusher kick vs hard
  throttle, deferred -EIO, and O_SYNC per-write durability.
- The bad example reproduces the "write() returned but data not durable"
  bug at runtime and exits 0 without crashing the harness.

| Example | Compile | Run | Observation |
|---|---|---|---|
| good/good_writeback.c | 0 | 0 | "ALL CHECKS PASSED" (7 semantics groups) |
| bad/bad_writeback.c | 0 | 0 | "BUG reproduced: write() returned but data not durable" |

Host: MinGW gcc 16.1.0 (MSYS2), Python 3.11, Windows/PowerShell. Executables
were written outside the repo (temp dir) and the repo tree was not modified
beyond the skill directory.

NOT verified on this host (documented targets, do NOT claim to have run):
kernel build with CONFIG_MM/CONFIG_BLOCK, QEMU boot, xfstests runs, power-
fail crash-consistency harness.

## Historical CVE evals (adversarial)

| CVE | Class | Fixture | Detect | Fix | Verify |
|---|---|---|---|---|---|
| CVE-2022-0847 | page cache aliasing (Dirty Pipe), flags not cleared | fs/splice.c pipe_buffer page from page cache | spliced page keeps page-cache flags; pipe write to a read-only file offset succeeds | clear/restore page-cache flags on the alias path (commit 9d2231c5d74e) | KASAN + reproducer writing into a read-only file after splice |
| CVE-2016-5195 | writeable private mapping race (Dirty COW), page-fault/COW of a page cache page | mm/memory.c do_wp_page / follow_page race | mapping writable while the COW copy of the page-cache page is in flight; racing write lands in read-only memory | serialize page fault with the COW / check the pte after acquiring the page lock (CWE-362) | KASAN + concurrent-writer reproducer |

Each eval: DETECT (find the missing page-cache/writeback guarantee) ->
EXPLAIN (which rule was violated) -> FIX (restore flags / serialize the
race) -> VERIFY (KASAN clean + reproducer).

## Synthetic evals

- easy/positive: write followed by checked `fsync_emu` must NOT be flagged.
- easy/negative: write with no fsync and a durability claim must be flagged.
- medium/negative: dirty-page counter used as a durability proof must be
  flagged.
- medium/negative: `fdatasync_emu` used where metadata (mtime) must survive
  must be flagged.
- hard/negative: writeback completion that unconditionally cleans a redirtied
  page must be flagged.
- hard/negative: unchecked fsync return after a failed writeback must be
  flagged.

## Adversarial evals

- Code that "passes" a host test where the flusher runs synchronously inside
  `write_emu` must not be declared correct for a real kernel where writeback
  is asynchronous (`wb_workfn`).
- Code whose durability claim is valid only because the file is smaller than
  the dirty_background_ratio and no flusher ever ran.
- A journaled-filesystem claim that ignores data=writeback ordering (data is
  not committed with metadata).
- An O_DSYNC path presented as if it synced mtime.

## False-positive evals (correct code must not be flagged)

- Write + `assert(fsync_emu(&f, 1) == 0)` — do NOT flag.
- Data-only log that uses `fdatasync_emu` and checks the essential metadata
  (size) — do NOT flag.
- A page redirtied during writeback that stays PG_DIRTY and is re-written —
  do NOT flag.
- Ephemeral temp files that deliberately skip fsync — do NOT flag.
- O_SYNC writes that are durable on return — do NOT flag.

## Verification commands

Host (self-contained stubs — recorded this run):

```
gcc -Wall -Wextra -Werror -O2 examples/good/good_writeback.c -o /tmp/good_writeback
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_writeback.c -o /tmp/bad_writeback
python tools/tokens/token_measure.py --check 2000 skills/kernel/page-cache-writeback-semantics
python tools/lint/skill_lint.py skills/kernel/page-cache-writeback-semantics/SKILL.md
```

Target (kernel) — documented only, NOT run here:

```
make defconfig && make -j$(nproc)   # CONFIG_MM + CONFIG_BLOCK
qemu-system-x86_64 -kernel arch/x86/boot/bzImage -append "console=ttyS0" -nographic
# inside the VM, xfstests on ext4:
#   generic/484  fsync integrity
#   generic/342  fdatasync semantics
#   generic/099  crash consistency under power loss (data=ordered)
# KASAN + syzkaller for the splice/pipe and fault/COW paths (CVE classes)
```

## Scoring

- precision: every flagged pattern maps to a real page-cache/writeback rule.
- recall: each bad snippet is detected (runtime BUG reproduction).
- FP-rate: good snippets produce zero flags.
