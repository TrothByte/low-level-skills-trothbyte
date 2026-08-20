# BPF CO-RE — Compile-Once, Run-Everywhere Reference

Primary sources: Cilium BPF and XDP Reference Guide (CO-RE chapter),
kernel `Documentation/bpf/btf.rst`, libbpf sources and docs, facebookincubator
bpf-notes. Format per rule:
RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE → VERIFICATION → SOURCE.

## 0. What CO-RE is and why it exists

- **RULE**: CO-RE (Compile-Once, Run-Everywhere) makes one compiled BPF object
  load and run on any kernel version. Kernel struct layouts, enum values, type
  sizes, and helper/function identities differ between versions; CO-RE moves
  the adaptation from compile time to load time. The program records *what*
  it accesses (field names, type names) in BTF debug info; at load, libbpf
  resolves each access against the target kernel's BTF (vmlinux BTF at
  `/sys/kernel/btf/vmlinux`, exported by `CONFIG_DEBUG_INFO_BTF`).
- **WHY AI GETS IT WRONG**: treats CO-RE as a runtime compatibility layer or a
  compiler optimization. It is neither: it is a *load-time relocation* stage
  between clang and the kernel verifier.
- **CORRECT REASONING**: three actors, in order: (1) clang `-target bpf -g`
  emits the program plus BTF + BTF.ext (relocation records) into the object;
  (2) libbpf (bpf_object__load) reads the records and patches the resolved
  offsets/sizes/enum values into the program image using the running kernel's
  vmlinux BTF; (3) only then the verifier checks the patched program. CO-RE
  failure surfaces between (2) and (3): "CO-RE relocation failed" or "can't
  find field/type/...".
- **VERIFICATION**: `bpftool prog load -d prog.o /sys/fs/bpf/prog` prints the
  relocated values; `llvm-objdump -S prog.o` shows the `preserve_access_index`
  annotations in the object.
- **SOURCE**: Cilium CO-RE docs; kernel Documentation/bpf/btf.rst (BTF.ext
  section); libbpf `bpf_core.c` (relocation resolution).

## 1. BTF: the type info that makes relocation possible

- **RULE**: BTF (BPF Type Format) is a compact type graph emitted as `.BTF`
  and `.BTF.ext` ELF sections. `.BTF` describes types (structs, unions, enums,
  funcs, func protos); `.BTF.ext` records where in the instruction stream each
  CO-RE relocation sits and which field/type/enum it references. `vmlinux.h`
  is just the kernel's own BTF rendered as C: `bpftool btf dump file
  /sys/kernel/btf/vmlinux format c > vmlinux.h`. A program compiled against it
  carries relocations whose names match the running kernel's BTF.
- **WHY AI GETS IT WRONG**: dropping `-g` "because release builds don't need
  debug info", or hand-writing kernel structs to avoid the include. Without
  BTF there are no relocations; with hand-declared structs the names match but
  the offsets come from the stale headers, which is worse (silent, not
  load-time-detected).
- **CORRECT REASONING**: the golden command is `clang -target bpf -g -O2 -c
  prog.c -o prog.o`. `-g` is what makes clang emit BTF; `-O2` keeps the
  program fast enough and is required in practice for the verifier. Check the
  object with `readelf -S prog.o` (`.BTF` and `.BTF.ext` present) and
  `bpftool btf dump file prog.o` to inspect the recorded types.
- **EXAMPLE** (bad): `clang -target bpf -O2` (no `-g`) → `readelf -S` shows no
  `.BTF`/`.BTF.ext` → `bpftool prog load` fails with `CO-RE relocation failed`
  or the program has no relocations at all.
- **COUNTEREXAMPLE** (good): `clang -target bpf -g -O2` and
  `#include <vmlinux.h>`; `readelf -S prog.o | grep BTF` lists both sections.
- **VERIFICATION**: target: `readelf -S prog.o`, `bpftool btf dump file
  prog.o`, `bpftool prog load -d`.
- **SOURCE**: kernel Documentation/bpf/btf.rst; Cilium CO-RE docs ("The BTF
  basics"); libbpf docs.

## 2. Field-offset relocation and safe reads

- **RULE**: A field access like `task->mm` compiles to a load from a
  constant offset. With CO-RE, clang emits that load with an embedded
  relocation record (field `task_struct.mm`), and libbpf patches the offset
  from the target kernel's BTF before verification. The C language marker is
  `__builtin_preserve_access_index`; `bpf_core_read()` and the `BPF_CORE_READ`
  macro family apply it for you. The raw read helpers
  `bpf_probe_read_kernel()`/`bpf_probe_read_user()` do NOT record relocations:
  use them only for addresses with no compile-time field path.
- **WHY AI GETS IT WRONG**: writing `p->field` and expecting relocation to
  happen "because CO-RE is on". A plain deref `p->field` (no
  preserve_access_index) bakes in the compiling kernel's offset; and in probe
  context (kprobe/tracepoint/fentry), page faults are disabled, so a deref of
  a kernel pointer that happens to fault panics the task instead of returning
  an error — the verifier does not reject it, the CPU does.
- **CORRECT REASONING**: rule of thumb — in tracing programs, every
  kernel-struct read routes through `BPF_CORE_READ(obj, a, b, c)` (relocatable
  chain) or `bpf_core_read(&dst, sizeof(dst), &src->field)`, never `*ptr`.
  For read-mostly fast paths the verifier can accept direct reads with
  `__builtin_preserve_access_index` on the source struct; but unless the
  pointer provenance is provably safe, use the read helpers.
- **EXAMPLE** (bad):
  ```c
  struct task_struct *t = bpf_get_current_task();
  u64 pid = t->pid;              /* plain deref, no relocation */
  struct mm_struct *mm = t->mm;  /* may fault: kprobe context, page faults off */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  u64 pid = BPF_CORE_READ(task, pid);              /* relocated offset */
  u64 rss = BPF_CORE_READ(task, mm, rss_stat, count); /* chain of relocations */
  ```
- **VERIFICATION**: target: `bpftool prog load -d` shows each relocated
  field; `llvm-objdump -S` shows `preserve_access_index` ops in the good
  object and none for the plain deref.
- **SOURCE**: Cilium CO-RE docs (bpf_core_read, BPF_CORE_READ);
  kernel/tools/lib/bpf/bpf_core_read.h; clang docs on
  `__builtin_preserve_access_index`.

## 3. Existence and value-guard helpers

- **RULE**: Not every field/type/enum exists on every kernel. The relocatable
  *oracle* helpers — `bpf_core_field_exists`, `bpf_core_type_exists`,
  `bpf_core_type_id_kernel`, `bpf_core_enum_value`,
  `bpf_core_enum_value_exists`, `bpf_core_type_size`, and the bitfield reader
  `BPF_CORE_READ_BITFIELD` — are resolved by libbpf at load. The dead-branch
  elimination in modern compilers removes guarded code when the test is
  constant at load; unguarded access to a missing field is a load-time
  failure.
- **WHY AI GETS IT WRONG**: assuming a field that exists on the develop kernel
  exists on production, or guarding with a runtime `if` that the loader cannot
  fold away (the relocation still fails at load).
- **CORRECT REASONING**: `if (bpf_core_field_exists(t->mm->rss_stat.count))`
  is evaluated by libbpf, not at runtime. On kernels where the field is
  missing, the whole branch is elided and the program loads. `bpf_core_enum_value`
  similarly resolves an enum value by name, and `bpf_core_type_size` the
  sizeof of a type that may differ between kernels. The guard must cover every
  access to the optional thing, including in helpers' arguments.
- **EXAMPLE** (bad):
  ```c
  /* field exists only on some kernels; no guard -> load fails on others */
  return BPF_CORE_READ(task, mm, rss_stat, count);
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (bpf_core_field_exists(task->mm->rss_stat.count))
      return BPF_CORE_READ(task, mm, rss_stat, count);
  return 0;  /* graceful degradation on kernels without the field */
  ```
- **VERIFICATION**: host model in `examples/good/core_reloc_model.py`
  demonstrates the guarded path on a v1/v2 pair; target: load the same object
  on kernels with and without the field.
- **SOURCE**: Cilium CO-RE docs (CO-RE helpers); libbpf
  `bpf_core_read.h`; kernel/tools/testing/selftests/bpf/progs/
  (core_reloc tests).

## 4. Hardcoded offsets: the anti-pattern

- **RULE**: Offsets taken from a disassembly, a doc, or `pahole` output of one
  kernel are constants at compile time. They produce a program that is correct
  on exactly one kernel and silently wrong (or faulting) on every other — no
  error at load, because there is no relocation to fail.
- **WHY AI GETS IT WRONG**: the value *looks* verified because it came from a
  real kernel, and the program loads cleanly on the test box.
- **CORRECT REASONING**: kernel struct layouts are not an ABI for BPF
  tracing. Any constant offset in BPF C is a code smell; replace it with the
  named access (BPF_CORE_READ / bpf_core_read / preserve_access_index) so the
  loader computes the offset. This also applies to `BPF_FIELD_BYTES`-style
  sizeof tricks derived from a fixed layout.
- **EXAMPLE** (bad): `struct mm_struct *mm = *(void **)((char*)task + 0x10);`
  — see `examples/bad/hardcoded_offset.c`.
- **COUNTEREXAMPLE** (good): `struct mm_struct *mm = BPF_CORE_READ(task, mm);`
  — see `examples/good/portable_read.c`.
- **VERIFICATION**: host model `examples/bad/reloc_misuse.py` shows the
  hardcoded offset reading the wrong field on a v2 layout; target: load the
  same object on two kernel versions and diff the relocated offsets with
  `bpftool prog load -d`.
- **SOURCE**: Cilium CO-RE docs (motivation section); facebook bpf-notes
  (CO-RE section).

## 5. CO-RE vs the verifier: two failure classes at load

- **RULE**: CO-RE relocation and verifier checks both happen at load time but
  are different stages. libbpf relocation runs first and fails with its own
  messages (`CO-RE relocation failed: ...`); the verifier runs second on the
  patched program and fails with register-state messages. A program can fail
  either way on different kernels.
- **WHY AI GETS IT WRONG**: debugging a relocation error as if it were a
  verifier rejection, or "fixing" a portability bug by adding null checks.
- **CORRECT REASONING**: message tells the stage. `libbpf: prog 'x': failed to
  relocate` / `can't find field` / `can't find type` / `can't find enum` →
  CO-RE issue (missing BTF, missing `-g`, unguarded optional field). Verifier
  strings like `R0 invalid mem access` → safety issue. The two coexist: a
  portably relocated read of an unchecked pointer still needs the verifier's
  rules; a verifier-safe plain deref still needs CO-RE to stay portable.
- **VERIFICATION**: run the same object on two kernel versions; note which
  stage rejects (or which silently mis-reads — the worst case).
- **SOURCE**: kernel Documentation/bpf/btf.rst; libbpf docs; Cilium CO-RE docs.

## 6. bpf_probe_read_kernel vs bpf_core_read

- **RULE**: `bpf_probe_read_kernel(dst, size, unsafe_ptr)` is the raw
  kernel-space probe read; it does not relocate the offset of the source
  field. `bpf_core_read(dst, size, src)` is the relocatable wrapper
  (it emits a `preserve_access_index` relocation for the source expression and
  calls the probe helper under the hood). Prefer `bpf_core_read` /
  `BPF_CORE_READ` for named fields; the bare helpers are for byte-offset or
  address-based reads where no field path exists.
- **WHY AI GETS IT WRONG**: using `bpf_probe_read_kernel` on every field read
  "because it is safe", which silently discards CO-RE: the offsets inside the
  expressions are still baked in from the compiling kernel's headers.
- **CORRECT REASONING**: if you can name the field, `BPF_CORE_READ` it; the
  probe helper still does the fault-safe read at runtime, but the offset was
  relocated at load. Reserve raw helpers for genuinely dynamic addresses.
- **VERIFICATION**: `llvm-objdump -S` on the object: `bpf_core_read` produces
  `preserve_access_index`-marked loads; a raw
  `bpf_probe_read_kernel(&dst, 8, &t->mm)` does not.
- **SOURCE**: bpf-helpers man pages; libbpf `bpf_core_read.h`; Cilium CO-RE
  docs.

## 7. Common load-time failures and their fixes

| libbpf / load error | Cause | Fix |
|---|---|---|
| `CO-RE relocation failed` | relocation target missing or ambiguous | check field path, guard with `bpf_core_field_exists` |
| `can't find field 'x' in type 'y'` | field missing on this kernel / stale headers | use vmlinux.h, guard the field |
| `can't find type 'x'` / `can't find enum` | type renamed/missing | `bpf_core_type_exists` / `bpf_core_enum_value` guard |
| no `.BTF`/`.BTF.ext` in object | compiled without `-g` | add `clang -target bpf -g` |
| `unknown relocation type` | BTF.debug info incomplete / wrong compiler | rebuild with BTF-capable clang (>= 11) |
| verifier rejection *after* relocation | the program itself is unsafe | that is the verifier skills' domain |

- **VERIFICATION**: reproduce each row on target; the python models reproduce
  the two structural causes (unguarded optional field, hardcoded offset) on the
  host.
- **SOURCE**: libbpf `libbpf.c` / `bpf_core.c` error strings; Cilium CO-RE
  docs troubleshooting; kernel selftests progs (core_reloc).
