# Evaluation — ebpf-verifier-reasoning

Skill: `skills/networking/ebpf-verifier-reasoning`. Stability target: `evaluated`.

## Model of evaluation

The unsafe fixtures are valid C that compiles with gcc and clang. Their
unsafety is only visible to the kernel verifier at program load, so each
synthetic eval is scored on three gates:
1. C sanity (host): the file compiles with `gcc -Wall -Wextra -Werror -O2`.
2. Target compile (Linux): `clang -O2 -g -target bpf -c` succeeds for both
   good and bad (compilation is NOT verification).
3. Target load (Linux): `bpftool prog load` — good fixtures load, bad fixtures
   are rejected with the exact verifier message in the table below.

## Synthetic evals (bad fixtures -> expected verifier message)

| Fixture | Rule | Expected verifier log line (target) |
|---|---|---|
| `examples/bad/bad_unbounded_loop.c` | bounded loops | `BPF program is too large. Processed <N> insn` (unprovable loop) |
| `examples/bad/bad_missing_null_check.c` | null check | `R0 invalid mem access 'map_value_or_null'` |
| `examples/bad/bad_map_index_oob.c` | map bounds | `R0 unbounded memory access, make sure to bounds check any such access` |
| `examples/bad/bad_packet_without_bounds.c` | packet bounds | `invalid access to packet, off=0 size=1, R3(id=0,off=0,r=0)` |

## Positive evals (good fixtures -> must load)

| Fixture | Rule exercised |
|---|---|
| `examples/good/good_bounded_loop.c` | loop bound clamped to a constant before the loop |
| `examples/good/good_null_check.c` | null check after `bpf_map_lookup_elem` on every path |
| `examples/good/good_map_bounds_check.c` | null check + bounded map-value index |

## False-positive evals (correct code must NOT be flagged)

- Bounded loop with explicit clamp `if (n > 64) n = 64;` — must NOT be rejected
  as an unbounded loop.
- `if (!v) return 0;` before `v->f += 1;` — must NOT be rejected as
  `map_value_or_null` access.
- `if (idx >= 8) return 0;` before `b->counters[idx]` — must NOT be rejected as
  unbounded memory access.

## Adversarial evals

- Null check present in only one branch of an `if/else` (deref in the other
  branch) — must be flagged (`R0 invalid mem access 'imm'`).
- Loop counter derived from packet data with the clamp written as a boolean
  expression the verifier cannot use (`n = min(n, 64);` via a helper call) —
  must be flagged as unprovable.
- Index expressed as `b->counters[idx & 7]` — verifier bounds the mask to
  [0, 8) only if `idx` is a 32-bit value with the mask applied before the
  pointer add; a 64-bit `idx` with unknown high bits may still be rejected.

## Verification commands

Host (any OS; this is a C-sanity gate, NOT verifier acceptance):

```
gcc -Wall -Wextra -Werror -O2 -c examples/bad/*.c examples/good/*.c
```

Target (Linux only; requires clang with BPF target + bpftool + root):

```
clang -O2 -g -target bpf -c examples/good/good_null_check.c -o /tmp/good.o
clang -O2 -g -target bpf -c examples/bad/bad_missing_null_check.c -o /tmp/bad.o
bpftool prog load /tmp/good.o /sys/fs/bpf/good        # must succeed
bpftool prog load -d /tmp/bad.o /sys/fs/bpf/bad       # must fail; -d prints verifier log
```

`bpftool prog load -d` prints the verifier log on failure. For runtime tracing
output (`bpf_trace_printk`), the bpftool subcommand is `bpftool prog tracelog`
(one word; there is no `bpftool prog trace log`).

Note: the fixture files declare their own helper stubs (`bpf_map_lookup_elem`,
`bpf_probe_read*`) in the style of libbpf's generated `bpf_helper_defs.h`. For a
target build, replace the stub block with `#include <bpf/bpf_helpers.h>`
(which provides the same declarations) and `#include <linux/bpf.h>` for
`struct xdp_md`/`struct iphdr`; the program bodies are unchanged.

## Verified facts

| Fact | Status | Evidence |
|---|---|---|
| gcc available, version 16.1.0 (MSYS2 ucrt64) | VERIFIED | `gcc --version` |
| clang available on host | UNVERIFIED / absent | `Get-Command clang` — not found |
| bpftool available on host | UNVERIFIED / absent | `Get-Command bpftool` — not found |
| all 7 fixtures compile with `gcc -Wall -Wextra -Werror -O2 -c`, exit 0 | VERIFIED | exit codes below |
| eBPF programs can be loaded/verified on this Windows host | NO | eBPF requires a Linux kernel |
| `clang -target bpf` compile + `bpftool prog load` verifier checks | documented-as-target | commands above; not executed here |
| `R0 invalid mem access 'map_value_or_null'` and the other messages in the table are the kernel's actual strings | VERIFIED (from source) | `Documentation/bpf/verifier.rst`, `kernel/bpf/verifier.c` (torvalds/linux master) |
| helper protos for `bpf_map_lookup_elem` / `bpf_probe_read*` | VERIFIED (from source) | `kernel/bpf/verifier.c`, `kernel/trace/bpf_trace.c`, `include/uapi/linux/bpf.h`, libbpf `bpf_helper_defs.h` |
| `MAX_BPF_STACK = 512` | VERIFIED (from source) | `include/linux/filter.h:100` |
| verifier exploration limit 1,000,000 insns | VERIFIED (from source) | `Documentation/bpf/bpf_design_QA.rst`, `kernel/bpf/verifier.c` |
| fixture compile exit codes (host, gcc 16.1.0) | VERIFIED | see below |

### Host compile results (gcc 16.1.0, MSYS2 ucrt64, executed 2026-08-14)

```
gcc -Wall -Wextra -Werror -O2 -c examples/bad/bad_unbounded_loop.c        -> exit 0
gcc -Wall -Wextra -Werror -O2 -c examples/bad/bad_missing_null_check.c    -> exit 0
gcc -Wall -Wextra -Werror -O2 -c examples/bad/bad_map_index_oob.c         -> exit 0
gcc -Wall -Wextra -Werror -O2 -c examples/bad/bad_packet_without_bounds.c -> exit 0
gcc -Wall -Wextra -Werror -O2 -c examples/good/good_bounded_loop.c        -> exit 0
gcc -Wall -Wextra -Werror -O2 -c examples/good/good_null_check.c          -> exit 0
gcc -Wall -Wextra -Werror -O2 -c examples/good/good_map_bounds_check.c    -> exit 0
```

## Scoring (for routing eval)

- precision: every flagged fixture maps to a real verifier rule and message.
- recall: each bad fixture must be detected by the verifier on the target.
- FP-rate: good fixtures must load cleanly; a false rejection on the bounded
  loop, null check, or bounds check scores a miss.
