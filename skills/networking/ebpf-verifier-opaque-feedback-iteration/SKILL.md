---
name: ebpf-verifier-opaque-feedback-iteration
description: Use when a BPF program fails the verifier with an opaque log: extract the failing instruction from the log tail, minimize the program, bisect register state, and iterate a minimal fix. Teaches persisting where LLMs give up.
---

# eBPF Verifier Opaque-Feedback Iteration

## When to use

- A BPF program is rejected at load with a multi-line verifier dump that is
  hard to read ("huge dumps of data that obscure the actual source of the
  problem").
- An LLM or agent has "given up" on a BPF program instead of producing a fix
  — the documented failure mode for BPF.
- Debugging which instruction / register state the verifier rejected, and
  iterating a minimal fix against the load loop.
- Minimizing a BPF program to the smallest form that still fails, to isolate
  the offending construct.

## When not to use

- Learning the verifier's safety model from scratch — use
  `ebpf-verifier-reasoning`.
- Post-load runtime behavior — that is not a verifier issue.
- Userspace programs that never hit the kernel verifier.

## What the agent often gets wrong

- "The log is too big; I cannot tell what failed; I'll rewrite the whole
  program." Starovoitov: verifier error output "are insane" — huge dumps that
  obscure the cause — and writing BPF code is the only case where LLMs give
  up rather than producing something. The log tail contains the answer.
- "The last line is the error." The tail `processed N insns (limit 1000000)`
  is a summary; the actual failure message and its instruction are a few lines
  above it, with the register state printed before the message.
- "I'll guess a fix from the message text." The message names a register
  (`R2 invalid access to packet`); the fix must change how THAT register's
  state was established, not a random nearby line.
- "A full rewrite is safer than a minimal change." The verifier rejects whole
  programs; minimizing to the failing slice makes the cause obvious and the
  fix verifiable.

## How to reason correctly

1. Extract from the log tail: the failure message, the failing instruction
   (the last instruction line before the message), and the register state
   printed just above the message (`R2(id=0,off=0,r=0)` etc.).
2. Minimize: remove instructions that do not change the failure signature
   (delta-debug); the minimal failing program is the question.
3. Bisect the state: find which instruction created the offending register's
   state (`r=0` on a `PTR_TO_PACKET` means no bounds established). That
   instruction names the missing construct.
4. Repair minimally: add the missing check (e.g. a `data_end` bounds
   comparison) immediately before the failing access — on the path the
   verifier sees.
5. Re-verify and repeat: each iteration shrinks the diff between "fails" and
   "loads". Never rewrite the whole program; never give up on a dump.
6. On a real target use `bpftool prog load -d` (the `-d` flag prints the
   verifier log) as the feedback loop.

## What to verify

- The failing instruction index and message come from the log, not from
  guessing.
- The register state in the message is traced back to its creating
  instruction.
- The minimal failing program preserves the failure signature.
- The fix is inserted before the failing access on the seen path.
- Re-verification is clean; the diff from the original is minimal.

## How to verify

```
# Python simulation of the iterate-on-opaque-log loop (plain python 3.11):
python examples/good/good_verifier_log_iteration.py
# Expected: extract -> minimize -> bisect -> repair -> re-verify PASS.

# The give-up failure mode:
python examples/bad/bad_give_up_on_opaque_log.py
# Expected: agent gives up on the opaque dump and produces no fix.

# C fixtures (host C sanity, gcc 16.1.0):
gcc -Wall -Wextra -Werror -O2 -c examples/good/good_bounds_checked.c
gcc -Wall -Wextra -Werror -O2 -c examples/bad/bad_casts_away_bounds.c

# Target (Linux only; documented-as-target, not executed here):
clang -O2 -g -target bpf -c examples/bad/bad_casts_away_bounds.c -o /tmp/bad.o
bpftool prog load -d /tmp/bad.o /sys/fs/bpf/bad   # -d prints the verifier log
clang -O2 -g -target bpf -c examples/good/good_bounds_checked.c -o /tmp/good.o
bpftool prog load /tmp/good.o /sys/fs/bpf/good    # must load cleanly
```

## Where the knowledge comes from

- `lwn-1075067` — BPF in the agentic era (LSFMM+BPF 2026): verifier errors
  "are insane", huge dumps obscure the cause; "register is not init" is too
  opaque; writing BPF is the only case where LLMs give up rather than
  producing something. (new source, proposed)
- `ebpf-docs` — kernel `Documentation/bpf/verifier.rst` (log format, register
  state lines) and `kernel/bpf/verifier.c` (message strings, `processed N
  insns` tail).
- `ebpf-verifier-reasoning` — the safety model behind the messages.

## Related skills

- `ebpf-verifier-reasoning` (extend) — this skill adds the opaque-log
  iteration loop on top of the verifier safety model.
- `meta-verification` (recommend) — verification is a loop; "no fix produced"
  is not a verdict, it is a stopping-rule violation.
- `debugging-crash-triage-discipline` (recommend) — extract-from-tail and
  minimize are the same discipline applied to verifier dumps.
- `fuzzing-harness-evidence-gate` (recommend) — the load loop needs a
  repeatable harness (`bpftool prog load -d`).

## Evaluation

Synthetic: `bad/bad_casts_away_bounds.c` (unchecked packet access) must be
identified via its verifier log and repaired minimally;
`bad/bad_give_up_on_opaque_log.py` must be rejected as a stopping-rule
violation; `good/good_bounds_checked.c` and `good/good_verifier_log_iteration.py`
demonstrate the correct loop.
False-positive: a program that loads cleanly must not be "repaired"; a fix
that already passes re-verification must not be rewritten.
Historical: Starovoitov's LSFMM+BPF 2026 statements — verifier output "are
insane", BPF is the only case LLMs give up — are the documented incident for
this failure mode.
Adversarial: a program whose log names a register whose bad state was created
several instructions earlier (bisection must find the creator); a minimized
program that still preserves the failure; a check inserted on the wrong path
must fail re-verification.
Recorded output: `evals/README.md` (Python simulation and gcc fixture
compiles actually executed on this host; bpftool documented-as-target).
