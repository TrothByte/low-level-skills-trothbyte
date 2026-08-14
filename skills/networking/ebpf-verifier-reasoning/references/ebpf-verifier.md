# eBPF Verifier — Safety Model Reference

Primary sources: Linux kernel `Documentation/bpf/verifier.rst`,
`kernel/bpf/verifier.c`, `kernel/trace/bpf_trace.c`, `include/uapi/linux/bpf.h`,
`include/linux/filter.h`, `include/linux/bpf_verifier.h` (all under the
`ebpf-docs` registry entry) and the libbpf `bpf_helper_defs.h` helper
declarations. Format per rule:
RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE → VERIFICATION → SOURCE.

## 0. The verifier's model

- **RULE**: The verifier runs in two steps: a CFG/DAG check that rejects
  unreachable instructions, then a symbolic simulation that tracks every
  register and stack slot over all reachable paths. Pointers carry a type
  (`PTR_TO_CTX`, `PTR_TO_MAP_VALUE`, `PTR_TO_MAP_VALUE_OR_NULL`, `PTR_TO_STACK`,
  `PTR_TO_PACKET`, `PTR_TO_PACKET_END`, `PTR_TO_SOCKET[_OR_NULL]`), a fixed
  offset, and a variable-offset range; scalars carry signed/unsigned min-max
  ranges and a tnum. R0 must be initialized before exit.
- **WHY AI GETS IT WRONG**: treats "the C compiler accepted it" as evidence of
  safety, or reasons about runtime values instead of the verifier's static
  state.
- **CORRECT REASONING**: the verifier is a proof checker over the abstract
  state it computes, not over runtime data. A check the verifier cannot see
  does not exist for it. If a state with an already-seen branch repeats exactly,
  the loop is an infinite loop; otherwise exploration is bounded by
  `BPF_COMPLEXITY_LIMIT_INSNS` (1,000,000).
- **EXAMPLE** (bad): assuming a C-level `if (p) use(p);` written after other
  statements still protects a deref of an unchecked `map_value_or_null` pointer.
- **COUNTEREXAMPLE** (good): writing the null check so the verifier sees the
  branch immediately before the dereference.
- **VERIFICATION**: `bpftool prog load -d` and read the log; the failing
  instruction prints its register state.
- **SOURCE**: ebpf-docs: Documentation/bpf/verifier.rst ("Register value
  tracking"); kernel/bpf/verifier.c; include/linux/bpf_verifier.h.

## 1. bpf_map_lookup_elem() returns PTR_TO_MAP_VALUE_OR_NULL

- **RULE**: `bpf_map_lookup_elem(map, key)` is declared with
  `.ret_type = RET_PTR_TO_MAP_VALUE_OR_NULL`, `.arg1_type = ARG_CONST_MAP_PTR`,
  `.arg2_type = ARG_PTR_TO_MAP_KEY`. It returns either a pointer to the map
  value or NULL. Dereferencing the result before a null check is rejected. After
  `if (v != 0)` the verifier turns the pointer into `PTR_TO_MAP_VALUE` in the
  true branch and `CONST_IMM` (0) in the false branch; the false branch must not
  use it as a pointer.
- **WHY AI GETS IT WRONG**: "ARRAY maps are pre-allocated, so lookup cannot
  fail" — true at runtime, irrelevant to the type rule; the verifier rejects
  the unchecked deref regardless of map type.
- **CORRECT REASONING**: the return type is `_OR_NULL`, and the verifier
  enforces the check structurally on every path. One null check is enough for
  all copies of the pointer (they share an `id`), but only within the checked
  branch.
- **EXAMPLE** (bad):
  ```c
  struct counter_t *v = bpf_map_lookup_elem(&map, &key);
  v->hits += 1;   /* verifier: R0 invalid mem access 'map_value_or_null' */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  struct counter_t *v = bpf_map_lookup_elem(&map, &key);
  if (!v)
      return 0;
  v->hits += 1;   /* now PTR_TO_MAP_VALUE on this path */
  ```
- **VERIFICATION**: target: `clang -O2 -g -target bpf -c` then
  `bpftool prog load`; log shows `R0 invalid mem access 'map_value_or_null'`
  for the bad form.
- **SOURCE**: ebpf-docs: Documentation/bpf/verifier.rst ("Understanding eBPF
  verifier messages"); kernel/bpf/verifier.c (check_reg_type).

## 2. BPF_MAP_TYPE_ARRAY bounds and map-value pointer arithmetic

- **RULE**: For `BPF_MAP_TYPE_ARRAY` the key is a u32 and the array size is
  fixed by `max_entries` at creation; all elements are pre-allocated,
  zero-initialized, and 8-byte aligned. The value size is known to the verifier
  at load. Any access through the returned pointer must satisfy
  `offset + size <= value_size`; a constant offset out of that range is
  rejected, and a variable offset must be provably bounded.
- **WHY AI GETS IT WRONG**: indexing a fixed-size array field inside the map
  value with an unchecked packet-derived scalar and expecting the map to "just
  return NULL" or C to catch it.
- **CORRECT REASONING**: the verifier knows `value_size` and checks every load
  against it. An unbounded scalar offset has no safe range, so the access is
  rejected even though the runtime array is inside a heap buffer. Bound the
  index first with a branch the verifier sees (`if (idx >= 8) return 0;`).
- **EXAMPLE** (bad):
  ```c
  struct bucket_t *b = bpf_map_lookup_elem(&map, &key);
  if (!b)
      return 0;
  u32 idx = xdp->rx_queue_index;      /* unbounded scalar */
  b->counters[idx] += 1;              /* verifier: unbounded memory access */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  u32 idx = xdp->rx_queue_index;
  if (idx >= 8)
      return 0;                       /* idx is now [0, 8) */
  b->counters[idx] += 1;              /* off+size provably <= value_size */
  ```
- **VERIFICATION**: target: verifier log `... unbounded memory access, make
  sure to bounds check any such access` or `invalid access to map value,
  value_size=%d off=%d size=%d`.
- **SOURCE**: ebpf-docs: Documentation/bpf/map_array.rst;
  kernel/bpf/verifier.c (check_map_access).

## 3. Pointer arithmetic rules

- **RULE**: A pointer may be adjusted by a provably bounded scalar
  (`ptr + const`, `ptr + bounded_scalar`); it may not be added to another
  pointer, and arithmetic is forbidden on `CONST_PTR_TO_MAP`,
  `PTR_TO_MAP_VALUE_OR_NULL`, `PTR_TO_PACKET_END`, and
  `PTR_TO_SOCKET[_OR_NULL]`. Adding an unbounded scalar to a pointer is
  rejected.
- **WHY AI GETS IT WRONG**: assuming any `p + n` in C is fine, or that
  pointer+pointer "adds offsets".
- **CORRECT REASONING**: `R2 = R1 + R1` with both pointers yields
  `SCALAR_VALUE` (an invalid pointer) per the doc model. The verifier only
  allows add/sub with a bounded operand; `+` of two pointers is not a usable
  address. Constants past `BPF_MAX_VAR_OFF` and scalars with unbounded minimums
  are rejected before the access check.
- **EXAMPLE** (bad):
  ```c
  struct bucket_t *b = bpf_map_lookup_elem(&map, &key);
  if (!b)
      return 0;
  u32 off = xdp->rx_queue_index;                 /* unbounded scalar */
  struct bucket_t *c = (void *)((unsigned long)b + off); /* arbitrary ptr math */
  return c->total;   /* rejected: no provable safe range for this address */
  ```
  (In BPF asm, `R2 = R1 + R1` with two pointers yields an unusable
  `SCALAR_VALUE`; C cannot express pointer+pointer directly.)
- **COUNTEREXAMPLE** (good):
  ```c
  if (data + sizeof(*h) > data_end)
      return 0;
  struct hdr *h = (struct hdr *)data;   /* fixed small offset from a checked base */
  ```
- **VERIFICATION**: target: log `math between %s pointer and register with
  unbounded min value is not allowed`, or `value %lld makes %s pointer be out
  of bounds`.
- **SOURCE**: ebpf-docs: Documentation/bpf/verifier.rst (register types);
  kernel/bpf/verifier.c (adjust_ptr_min_max_vals).

## 4. Direct packet access requires a visible bounds check

- **RULE**: To dereference packet data, load `skb->data` and `skb->data_end`,
  verify `data + len <= data_end` with a branch, and only then access through
  `data`. Without the check the verifier cannot assign a safe range and rejects
  the load. Only add/sub are allowed on packet registers; other operations
  demote them to `SCALAR_VALUE`.
- **WHY AI GETS IT WRONG**: casting `ctx->data` to a struct pointer and reading
  a field directly, relying on the "packet is at least that long in practice".
- **CORRECT REASONING**: the verifier tracks packet pointers as
  `pkt(id, off, r)` where `r` is the proven safe range. The range only grows
  from a compare against `PTR_TO_PACKET_END` that the verifier sees. If the
  added scalar is wider than 16 bits the range information is lost entirely and
  any read gives `invalid access to packet`.
- **EXAMPLE** (bad):
  ```c
  void *data = xdp->data;
  struct iphdr *ip = (struct iphdr *)data;
  return ip->protocol;   /* no data_end check: invalid access to packet */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  void *data = xdp->data;
  void *data_end = xdp->data_end;
  if (data + sizeof(struct iphdr) > data_end)
      return 0;
  return ((struct iphdr *)data)->protocol;
  ```
- **VERIFICATION**: target: log `invalid access to packet, off=%d size=%d,
  R%d(id=...,off=...,r=...)`.
- **SOURCE**: ebpf-docs: Documentation/bpf/verifier.rst ("Direct packet access");
  kernel/bpf/verifier.c (check_packet_access).

## 5. Bounded loops

- **RULE**: Loops are allowed only when the verifier can prove the iteration
  count is bounded from static state — a constant limit, or a counter clamped by
  a branch (`if (n > 64) n = 64;`). An unbounded loop is rejected: either its
  state repeats (infinite loop) or exploration hits `BPF_COMPLEXITY_LIMIT_INSNS`
  (1,000,000), reported as `BPF program is too large. Processed %d insn`.
- **WHY AI GETS IT WRONG**: "the loop terminates for real packet sizes, so it is
  fine" — the verifier must prove it without executing.
- **CORRECT REASONING**: the verifier simulates each iteration with new state;
  a bound must come from a value whose range it knows (constant, or a scalar it
  has clamped). The `bpf_loop` helper is the recommended bounded-iteration
  construct and itself enforces an iteration limit
  (`frame%d bpf_loop iteration limit reached`).
- **EXAMPLE** (bad):
  ```c
  u64 n = xdp->rx_queue_index;   /* unbounded scalar */
  for (u64 i = 0; i < n; i++)
      sum += i;                  /* verifier: loop not provably bounded */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  u64 n = xdp->rx_queue_index;
  if (n > 64)
      n = 64;                    /* clamp gives umax=64 */
  for (u64 i = 0; i < n; i++)
      sum += i;                  /* bounded, accepted */
  ```
- **VERIFICATION**: target: log `BPF program is too large. Processed %d insn`
  or an infinite-loop state error for the bad form; clean load for the good one.
- **SOURCE**: ebpf-docs: Documentation/bpf/bpf_design_QA.rst (verifier limits);
  kernel/bpf/verifier.c (complexity limit, bpf_loop limit);
  include/linux/bpf_verifier.h (loop detection).

## 6. Stack access and the frame pointer

- **RULE**: R10 is the read-only frame pointer of type `PTR_TO_STACK`. Stack
  bounds are `[-MAX_BPF_STACK, 0)` with `MAX_BPF_STACK = 512`. The verifier
  only allows reads from stack slots that were previously written, and rejects
  offsets outside the range and variable (unbounded) offsets.
- **WHY AI GETS IT WRONG**: using `r10 + 8` (positive offset) or reading a
  local before initializing it, or indexing a local array with an unbounded
  scalar.
- **CORRECT REASONING**: stack slots are tracked like registers; an indirect
  read (passing a stack address to a helper, e.g. as a map key) requires the
  whole `key_size` region to be initialized. Variable offsets into the stack are
  prohibited for unprivileged programs.
- **EXAMPLE** (bad): `long x; bpf_map_lookup_elem(&map, &x);` without writing
  `x` first — log: `invalid indirect read from stack off -8+0 size 8`.
- **COUNTEREXAMPLE** (good): initialize the key first:
  `u32 key = 0; bpf_map_lookup_elem(&map, &key);`.
- **VERIFICATION**: target: log `invalid stack off=%d size=%d`, `invalid
  indirect read from stack off %d+%d size %d`, or `R%d variable stack access
  prohibited for !root`.
- **SOURCE**: ebpf-docs: Documentation/bpf/verifier.rst (register types);
  include/linux/filter.h (MAX_BPF_STACK 512); kernel/bpf/verifier.c.

## 7. Helper signatures: bpf_probe_read vs bpf_probe_read_kernel vs bpf_probe_read_user

- **RULE**: All three have the signature `(void *dst, u32 size, const void
  *unsafe_ptr)` with proto `.arg1_type = ARG_PTR_TO_UNINIT_MEM,
  .arg2_type = ARG_CONST_SIZE_OR_ZERO, .arg3_type = ARG_ANYTHING` and
  `.gpl_only = true`. The verifier requires `dst` to be a writable buffer and
  `size` to be a known constant or zero; the third argument (the source) is
  deliberately unchecked — the helper performs the safe probe at runtime.
- **WHY AI GETS IT WRONG**: assuming the source pointer also needs verifier
  validation, or that `bpf_probe_read` and `bpf_probe_read_kernel` are
  interchangeable.
- **CORRECT REASONING**: `bpf_probe_read_kernel` (helper id 113) probes kernel
  memory via `bpf_probe_read_kernel_common`; `bpf_probe_read_user` (id 112)
  probes user memory via `copy_from_user_nofault`; the legacy `bpf_probe_read`
  (id 4) reads kernel memory per its doc and, where
  `CONFIG_ARCH_HAS_NON_OVERLAPPING_ADDRESS_SPACE` is set, dispatches by address
  (user if `ptr < TASK_SIZE`); without that config it is not exposed at all.
  The docs say to prefer the `_kernel`/`_user` variants. All three return 0 or a
  negative error and zero `dst` on error. A helper may be absent for a given
  program type (`get_func_proto()` gates them), which surfaces as
  `program of this type cannot use helper ...` at load.
- **EXAMPLE** (bad): reading a user-space address with the kernel-space helper.
  The verifier accepts this (the source pointer is `ARG_ANYTHING`), so the bug
  only fails at runtime with `-EFAULT` on architectures without overlapping
  address spaces.
  ```c
  char buf[16];
  long ret = bpf_probe_read_kernel(buf, sizeof(buf), user_ptr);
  /* accepted by the verifier; fails at runtime: wrong address space */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  char buf[16];
  long ret = bpf_probe_read_user(buf, sizeof(buf), user_ptr);
  if (ret < 0)
      return 0;                     /* buffer was zeroed; do not treat as data */
  ```
- **VERIFICATION**: target: log `R1 type=%s expected=...` if `dst` is not a
  writable buffer, or a size error if `size` is not a known constant;
  `program of this type cannot use helper bpf_probe_read_kernel#113` when the
  helper is gated off for the program type.
- **SOURCE**: ebpf-docs: kernel/trace/bpf_trace.c (probe_read_* protos);
  include/uapi/linux/bpf.h (helper ids and doc text); libbpf bpf_helper_defs.h
  (declarations).

## 8. Reading verifier rejection messages

- **RULE**: Verifier messages name the failing rule. Map the message to the
  construct, fix the construct, and re-load.
- **WHY AI GETS IT WRONG**: parsing only the last line, or not seeing that the
  message refers to a specific register state printed just above.
- **CORRECT REASONING**: the log prints the instruction, then the register
  state, then the message. The register name in the message is the pointer or
  scalar at fault. Exact strings can vary across kernel versions; the table uses
  the master (6.x/7.x) wording.

| Message (as printed by the kernel) | Cause | Fix |
|---|---|---|
| `R0 invalid mem access 'map_value_or_null'` | deref of unchecked lookup result | add `if (!v) return ...;` |
| `R0 invalid mem access 'imm'` | pointer null-checked in one branch, used in the other | check on every path |
| `R0 !read_ok` / `R2 !read_ok` | register not initialized before read/exit | init before use |
| `invalid access to map value, value_size=%d off=%d size=%d` | constant offset past value size | bound the offset |
| `%s unbounded memory access, make sure to bounds check any such access` | variable offset with no provable range | `if (idx >= N) return 0;` |
| `invalid access to packet, off=%d size=%d, ...` | packet load with no/weak data_end check | check `data + len <= data_end` |
| `BPF program is too large. Processed %d insn` | unbounded loop / state explosion | clamp loop bound or use `bpf_loop` |
| `invalid indirect read from stack off %d+%d size %d` | uninitialized stack passed to helper (e.g. key) | initialize the key |
| `invalid stack off=%d size=%d` | stack offset outside [-512, 0) | use `r10 + (negative offset)` |
| `R%d variable stack access prohibited for !root` | unbounded scalar stack index | bound or restructure |
| `math between %s pointer and register with unbounded min value is not allowed` | unbounded scalar added to a pointer | bound the scalar first |
| `R1 type=%s expected=...` | helper argument type mismatch (e.g. `_OR_NULL` passed in) | null-check before the call |
| `misaligned access off %d size %d` | unaligned load/store | align access to size |
| `Unreleased reference id=%d alloc_insn=%d` | socket ref not released (older kernels: `id=1, alloc_insn=7`) | call the release helper |
| `invalid func %s#%d` | helper id not in range (older wording: `unknown func`) | use a real helper |
| `program of this type cannot use helper %s#%d` | helper gated off for this prog type | use the right prog type/helper |

- **VERIFICATION**: run the bad fixtures and diff the log against the table;
  each fixture must hit the message in its row.
- **SOURCE**: ebpf-docs: Documentation/bpf/verifier.rst ("Understanding eBPF
  verifier messages"); kernel/bpf/verifier.c (message strings).
