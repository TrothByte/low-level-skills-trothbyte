---
name: ebpf-verifier-reasoning
description: Use when writing, reviewing, or debugging eBPF C programs — map_lookup_elem null checks, bounded loops, pointer arithmetic on packet/map/stack pointers, helper argument constraints, and reading verifier rejection logs. Teaches how the kernel verifier proves safety and which patterns it rejects at load time.
---

# eBPF Verifier Reasoning

## When to use

- Writing eBPF C for XDP, tc, kprobe, or tracepoint programs that call helpers,
  read maps, or walk packet data.
- Reviewing a program rejected at load time with a verifier log line such as
  `R0 invalid mem access`, `invalid access to map value`, `unbounded memory
  access`, or `BPF program is too large`.
- Deciding whether a loop, a map-value pointer, or a packet-data pointer is
  provably safe in the verifier's model.
- Choosing between `bpf_probe_read_kernel` and `bpf_probe_read_user` and
  checking helper argument constraints.

## When not to use

- Userspace code or non-BPF kernel modules — they use a different safety model.
- Debugging a program that already *loaded*: post-load behavior bugs are runtime
  issues, not verifier issues.
- Compilation itself: `clang -target bpf` accepts the unsafe programs; only the
  kernel verifier rejects them, at program load on Linux.

## What the agent often gets wrong

- "It compiles with clang -target bpf, so it's safe." Compilation and
  verification are separate; rejection happens at load, not build time.
- "map_lookup_elem never fails for ARRAY maps, so no null check is needed."
  The helper still returns `PTR_TO_MAP_VALUE_OR_NULL`; the verifier requires a
  null check on every path before dereferencing.
- "I can index the array inside a map value with any number; C does not
  complain." The verifier must prove `off + size <= value_size`; an unbounded
  scalar offset is rejected (`unbounded memory access`).
- "A for loop that ends in practice is bounded." The bound must be provable from
  verifier state (a constant, or a scalar clamped by a visible branch), not from
  runtime reality.
- "Packet data is just a pointer." Direct packet access requires a bounds check
  the verifier can see (`data + len <= data_end`) before any load through it.

## How to reason correctly

1. Model the program the way the verifier does: every pointer has a type
   (`PTR_TO_MAP_VALUE`, `PTR_TO_PACKET`, `PTR_TO_STACK`, ...), a fixed offset,
   and a variable-offset range.
2. For every memory access ask: what is the object, is the offset provably in
   bounds, was an `_OR_NULL` type null-checked, is alignment known?
3. For every loop ask: does the state constrain the counter so termination is
   provable — a constant bound, or a clamp like `if (n > 64) n = 64;`?
4. Check helper calls against the helper's declared argument constraints
   (writable destination, constant or zero size, unchecked source pointer), and
   handle `RET_PTR_TO_MAP_VALUE_OR_NULL` return types with a null test.
5. Read the verifier log from the failing instruction: the log prints the
   register state and the message names the rule that failed.

## What to verify

- Every map-value pointer is null-checked before use, on every path.
- Every map-value, packet, and stack access has a provable bound and alignment.
- Every loop provably terminates (bounded counter).
- Helper arguments match the declared proto (dst writable, size known).
- The program loads on the target kernel on Linux; a clang compile alone proves
  nothing about verifier acceptance.

## How to verify

```
gcc -Wall -Wextra -Werror -O2 -c examples/good/*.c examples/bad/*.c   # C sanity, any OS
# On Linux (target only):
clang -O2 -g -target bpf -c examples/good/good_null_check.c -o /tmp/good.o
bpftool prog load /tmp/good.o /sys/fs/bpf/good
clang -O2 -g -target bpf -c examples/bad/bad_missing_null_check.c -o /tmp/bad.o
bpftool prog load -d /tmp/bad.o /sys/fs/bpf/bad     # rejected; -d prints the verifier log
```

## Where the knowledge comes from

- `ebpf-docs`: Documentation/bpf/verifier.rst (register types, direct packet
  access, verifier messages), Documentation/bpf/map_array.rst,
  Documentation/bpf/bpf_design_QA.rst (verifier limits)
- `ebpf-docs`: kernel sources — kernel/bpf/verifier.c (checks and rejection
  messages), kernel/trace/bpf_trace.c (probe_read_* protos),
  include/uapi/linux/bpf.h (helper ids and docs), include/linux/filter.h
  (MAX_BPF_STACK), include/linux/bpf_verifier.h (loop detection)
- libbpf bpf_helper_defs.h — the exact helper declarations used in the examples
- `kernel-coding-style` — example code style

## Related skills

- `kernel-uaccess-safety` — copy_to_user/copy_from_user semantics behind
  `bpf_probe_read_user` (recommend)
- `kernel-rcu-memory-barriers` — ordering inside BPF helpers and maps (recommend)
- `c-undefined-behavior` — C UB rules still apply inside the BPF C subset (require)

## Evaluation

Synthetic: 4 bad + 3 good fixtures (see `evals/README.md`); each bad fixture maps
1:1 to a real verifier log message.
False-positive: bounded loop with clamp, null-checked map value, and
bounds-checked map-value index must NOT be flagged.
Adversarial: unbounded scalar index into a map value array; null check on only
one branch.
Verified on this host: all 7 fixtures compile with `gcc -Wall -Wextra -Werror -O2`
(exit 0). Target verification (`clang -target bpf` + `bpftool prog load`) is
documented-as-target, not executed (Windows host, no clang/bpftool).
