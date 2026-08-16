# Evaluation — ebpf-verifier-opaque-feedback-iteration

Skill: `skills/networking/ebpf-verifier-opaque-feedback-iteration`.
Stability: `researched` (source-backed grounding: lwn-1075067 — LWN article
fetched and verified 2026-08-17; ebpf-docs). BPF toolchain (clang -target bpf,
bpftool, Linux kernel) is NOT available on this host; the iteration loop is
demonstrated by a Python 3.11 simulation actually executed here, and the C
fixtures compile with gcc (C sanity only). Mark: SIMULATED — models the
verifier log format; `bpftool prog load -d` documented-as-target.

## Synthetic evals

| Case | Fixture | Expected | Status |
|---|---|---|---|
| negative (give-up) | `bad/bad_give_up_on_opaque_log.py` | agent produces NO fix on an opaque dump — stopping-rule violation | RUN on host |
| negative (unchecked access) | `bad/bad_casts_away_bounds.c` | packet access without data_end; verifier rejects (target); gcc compiles | gcc exit 0; verifier target-only |
| positive (loop) | `good/good_verifier_log_iteration.py` | extract -> minimize (5->2) -> bisect -> repair -> re-verify PASS | RUN on host |
| positive (fixed C) | `good/good_bounds_checked.c` | bounds check before the load; loads cleanly (target) | gcc exit 0; verifier target-only |

## False-positive evals (correct code must NOT be flagged)

- `good/good_bounds_checked.c` — the bounds check is on the path the verifier
  sees, immediately before the access; must NOT be "repaired" again.
- A program that loads cleanly must not be rewritten; the loop terminates on
  the first clean re-verification.
- The minimal failing program `[data r1, ldw r0, [r1+0]]` must NOT be flagged
  as "too small to be the real program" — minimization is the point.

## Historical evals

- Starovoitov at LSFMM+BPF 2026 (lwn-1075067): verifier errors "are insane" —
  huge dumps that obscure the actual source of the problem — and writing BPF
  code is the only case he has observed where LLMs give up rather than
  producing something. Also: messages like "register is not init" are too
  opaque; the register state in the dump is the real diagnostic. These
  statements are the documented incident for this failure mode.

## Adversarial evals

- A program whose log names a register whose bad state (`r=0`) was created
  several instructions earlier — the bisect step must find the creating
  instruction (the `ctx->data` load), not the failing load.
- A minimized program must preserve the failure signature (message + named
  register) even though absolute instruction indices shift.
- A "fix" that inserts a bounds check after the failing load (or on a branch
  the verifier does not see) must fail re-verification — the loop's feedback
  is authoritative.
- The give-up response must be rejected even though the dump is multi-line
  and ends in the `processed N insns` summary.

## Verification commands

```
python examples/good/good_verifier_log_iteration.py
python examples/bad/bad_give_up_on_opaque_log.py
gcc -Wall -Wextra -Werror -O2 -c examples/good/good_bounds_checked.c
gcc -Wall -Wextra -Werror -O2 -c examples/bad/bad_casts_away_bounds.c
```

Target (Linux; documented-as-target, not executed here):

```
clang -O2 -g -target bpf -c examples/bad/bad_casts_away_bounds.c -o /tmp/bad.o
bpftool prog load -d /tmp/bad.o /sys/fs/bpf/bad     # -d prints the verifier log
clang -O2 -g -target bpf -c examples/good/good_bounds_checked.c -o /tmp/good.o
bpftool prog load /tmp/good.o /sys/fs/bpf/good      # must load cleanly
```

## Verified facts

| Fact | Status | Evidence |
|---|---|---|
| python 3.11.9 runs the iteration loop: extract -> minimize (5->2 insns) -> bisect -> repair -> re-verify PASS | VERIFIED (executed 2026-08-17) | output below |
| `bad_give_up_on_opaque_log.py` produces no fix on an 8-line dump | VERIFIED (executed) | output below |
| both C fixtures compile with `gcc -Wall -Wextra -Werror -O2 -c`, exit 0 | VERIFIED (executed) | exit codes below |
| verifier error output described as "insane" / huge dumps obscuring the cause | KNOWN (article fetched) | lwn-1075067 |
| BPF is the only case where LLMs give up rather than producing something | KNOWN (article fetched) | lwn-1075067 |
| "register is not init" style messages are too opaque; register state is the diagnostic | KNOWN (article fetched) | lwn-1075067 |
| `bpftool prog load -d` prints the verifier log on this host | UNVERIFIED / absent | no Linux kernel/bpftool |
| verifier log format (`processed N insns`, register-state lines, message strings) | KNOWN (from kernel source) | ebpf-docs: Documentation/bpf/verifier.rst, kernel/bpf/verifier.c |

### Host run (python 3.11.9, executed 2026-08-17)

`python examples/good/good_verifier_log_iteration.py`:

```
step 1 (extract): failing insn = 3, message = '3: invalid access to packet, off=0 size=8, R1(id=0,off=0,r=0)'
                  register state = '3: R1 type=PTR_TO_PACKET(id=0,off=0,r=0)'
step 2 (minimize): 5 insns -> 2 insns
                  minimal failing program: [('data', 1), ('ldw', 0, 1, 0)]
step 3 (bisect): r1 state = PTR_TO_PACKET(id=0,off=0,r=0) created at insn 0 (ctx->data load)
step 4 (repair): insert ctx->data_end + bounds check before the failing load
                  fixed program: [('data', 1), ('end', 9), ('check', 1, 9, 8), ('ldw', 0, 1, 0)]
step 5 (re-verify): PASS -- program loads cleanly
```

`python examples/bad/bad_give_up_on_opaque_log.py`: agent returns `fix = None`
("the verifier dump is too opaque to parse... regenerating the whole program")
while the failing instruction is at line 5, the register state at line 6, and
the message at line 7 of the dump.

`gcc -Wall -Wextra -Werror -O2 -c` -> exit 0 for both fixtures
(`good_bounds_checked.c`, `bad_casts_away_bounds.c`). Compilation is NOT
verification: the bad fixture compiles cleanly while the kernel verifier would
reject it at load.

## Scoring (for routing eval)

- recall: give-up response and unchecked packet access detected.
- precision: correct iteration loop and fixed C fixture produce no false
  flags.
- FP-rate: no false positives on the minimal program or the fixed fixture.
