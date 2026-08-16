# Evaluation — toctou-kernel

Skill: `skills/kernel/toctou-kernel`. Type: unique.
Stability: researched (Python race model deterministically executed on this
host; Linux `openat2` target not run — Windows host).

## Synthetic evals

| Case | Fixture | Expected | Status |
|------|---------|----------|--------|
| Locked check+use is atomic | `examples/good/atomic_check_use.py` | no inconsistent observation | runs: "no inconsistent observation (window closed)" |
| Check/use split across locks | `examples/bad/check_then_use.py` | TOCTOU observed | runs: "TOCTOU OBSERVED..." |
| `access()`+`open()` idiom | reasoning case | must be replaced by openat2 | eval case |
| Copy-then-validate for user ptr | reasoning case | approved | eval case |

## False-positive evals (correct code that must NOT be flagged)

- A lookup with `get_task_struct` held across use (refcount closed the
  window) — legal.
- `openat2` with `RESOLVE_BENEATH` + `RESOLVE_NO_MAGICLINKS` — the atomic
  pattern, must be approved.
- A single `openat` that never re-checks the path afterward (the fd is the
  authoritative handle) — no TOCTOU.
- A `copy_from_user`-then-validate where the pointer is never reused — legal.

## Historical evals

- **CWE-367 canonical example** — `access()`+`open()` race, used in real
  exploit chains against setuid binaries (documented in CWE-367).
- **`/tmp` symlink race class** — predictable temp file name + symlink swap;
  fixed by `O_CREAT|O_EXCL`, `mkstemp`, and `openat2(RESOLVE_NO_SYMLINKS)`.
- **Linux `stat`/`open` TOCTOU** — the same pattern in kernel interfaces
  (e.g., `fs/namei.c` hardening for `AT_EMPTY_PATH`); agent must explain why
  the atomic `*at` operations replace the two-step idiom.

## Adversarial evals (compiles-but-wrong)

- `access(path)` + `open(path)` compiles and "works in testing" — the race
  must be flagged without a runtime failure.
- A lock released between check and use (the "almost atomic" pattern).
- Revalidation of a mutable object under a different lock than the mutator
  uses.

## Verification commands

Host (executed on this host):

```
python3 examples/good/atomic_check_use.py
python3 examples/bad/check_then_use.py
```

Target (documented, Linux host needed — not run here):

```
gcc -O2 examples/good/openat2_demo.c -o /tmp/openat2 && /tmp/openat2
# race stress: run check_then_use in a loop with rename happening in another shell
# kernel: syzkaller with a driver under test; lockdep enabled
```

## Verified facts (KNOWN / INFERRED / UNVERIFIED)

- KNOWN: `atomic_check_use.py` runs on this host and prints the closed-window
  message (no inconsistent observation).
- KNOWN: `check_then_use.py` runs on this host and deterministically reports
  `TOCTOU OBSERVED: check passed on 'safe/file' but use opened 'opened
  attacker/replaced (current value)'` — the window is demonstrated by forcing
  the interleaving with events (actual run, 2026-08-17).
- INFERRED: `openat2` with RESOLVE_BENEATH is atomic on Linux 5.6+
  (researched from `kernel-source`; not run — Windows host).
- UNVERIFIED: real kernel path-resolution race reproduction (needs Linux).

## Scoring

- Precision: high — the model and structural reasoning are executable.
- Recall: high for the three documented patterns (path, user-pointer, object
  lookup); hardware/OS-specific reproduction is UNVERIFIED.
- FP-rate: low — atomic idioms (openat2, refcount-held lookups) are
  distinguishable.

## Tooling availability (honest)

- Available on this host: python 3.11.9 (race model executed).
- NOT installed: Linux kernel/syscall environment for `openat2`. Documented
  as a target command, not executed here.
