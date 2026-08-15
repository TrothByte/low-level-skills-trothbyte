---
name: riscv-cheri-capability-safety
description: Use when writing or reviewing capability-aware C/C++ for CHERI (Morello, CHERI-RISC-V): capability pointers, bounds and tags, purecap vs hybrid ABI, sealing/unsealing, CheriBSD, and CHERI-enabled sanitizers. Prevents out-of-bounds dereferences, tag loss, and false positives that flag correct bounded code.
---

# RISC-V CHERI Capability Safety

## When to use

- Writing C/C++ that will run under a pure-capability (purecap) CHERI ABI
  (Morello, CHERI-RISC-V) or CHERI-enabled clang.
- Reviewing code for CHERI correctness: bounds, permissions, tags, provenance.
- Distinguishing safe capability-derived sub-pointers from dangerous ones.
- Debugging CheriBSD faults (tag mismatch, bounds violation, permission error).
- Using CHERI's hardware memory safety as a sanitizer (hardware-assisted ASan).

## When not to use

- Non-CHERI C/C++ — normal ASan/UBSan apply; no capability model.
- CHERI kernel/MMU internals (capability CSRs, `CSR_CTR`-level code) — a separate
  low-level domain.
- RISC-V base ISA without the capability extension — use `riscv-isa-and-rvv-intrinsics`.
- Formal CHERI model proofs — the ISA semantics are the authority, not this skill.

## What the agent often gets wrong

- "A capability is just a fat pointer." No — it also carries permissions (load,
  store, execute, load/store-capability), a tag bit, provenance, and sealing.
  Reading a pointer as a plain integer clears the tag; capability-only stores
  require `StoreCap` permission.
- "Pointer arithmetic within bounds is always safe to keep." Any arithmetic that
  escapes the capability's bounds clears the tag; the derived pointer becomes
  unusable even if the *address* would have been in-bounds.
- "`memcpy` from a non-capability buffer is fine." Copying capabilities with
  byte-wise operations loses the tag — the result dereferences fault.
- "Purecap vs hybrid is just a compile flag." Purecap makes all pointers
  capabilities (larger ABI, new calling convention); hybrid keeps a separate
  non-capability address space. The ABIs differ in pointer size, alignment, and
  calling conventions.
- "Bounded sub-pointers are dangerous." They are the *intended* CHERI pattern:
  `csetbounds`/`cheri_bounds_set` creating a sub-capability is correct when the
  bounds cover the object. The FP risk is flagging every bounded pointer.
- "`cheri_perms_and`/permission checks are optional." Without the right
  permissions a valid capability faults on access; missing `LoadCap`/`StoreCap`
  is a real bug.
- "Tags survive through `reinterpret_cast` to integer." They do not: casting to
  integer and back drops the tag.

## How to reason correctly

1. For every pointer, enumerate its four properties: address, bounds (base/length),
   permissions, and tag. A fault occurs if the *address is out of bounds* OR a
   *required permission is missing* OR the *tag is cleared*. This is the complete
   failure model.
2. Track provenance: the tag is created by the allocator/loader; every operation
   that copies bytes, does arithmetic outside bounds, or casts through an integer
   must be checked for tag preservation.
3. To create a sub-pointer safely: `cheri_bounds_set(p, size)` then
   `cheri_offset_set(p, off)` — keep the result within the original bounds, and
   keep a permission mask that still permits the intended access.
4. For pointer equality/ordering and hashing, use `cheri_address_get(p)` (the
   numeric address) — never cast to `uintptr_t` (that's the whole capability).
5. Choose the ABI consciously: purecap (all pointers capabilities) vs hybrid
   (capability + non-capability address spaces); the calling convention and
   alignment differ.
6. In reviews, treat "valid capability but wrong bounds/permissions/tag" as three
   distinct bug classes, each with its own fix.

## What to verify

- Bounds: every derived pointer's bounds cover the full access range; no `csetbounds`
  with a length that truncates the object.
- Permissions: LoadCap/StoreCap present for capability loads/stores; the derived
  pointer's permission mask is a subset that still permits the operation.
- Tag: no byte-wise copy of capability storage; no cast to integer and back;
  `memcpy` of capability arrays done with capability-aware copies.
- ABI: purecap/hybrid choice consistent; pointer size/alignment per the CHERI
  ABI; variadic and `va_list` handling.
- Sealing: sealed capabilities only passed to the right `unseal` key.
- Correct code: bounded sub-pointers within bounds must NOT be flagged (FP rule).

## How to verify

```
# Target toolchain (documented; no CHERI build on this machine):
#   QEMU-CHERI + cheribuild:
cheribuild qemu-cheri --run /path/to/example   # or:
cheribuild run-fett
#   Compile a purecap binary:
cheribuild run-sdk --cheribsd -- purecap-cc -O2 examples/good/good_bounded.c
#   Expected: good_bounded runs clean; bad examples trap with a CHERI tag/bounds
#   fault (SIGPROT / EXC_CHERI in CheriBSD).
```

Toolchain status: no QEMU-CHERI / cheribuild on this machine. The `.c` examples
are documentary (researched — toolchain not available; command: `cheribuild
run-sdk --cheribsd -- purecap-cc`). A Python model of the capability
bounds/permission/tag enforcement logic was run; output in `evals/README.md`.

## Where the knowledge comes from

- `cheri-spec` — CHERI ISA and C/C++ programming guide: capabilities, bounds,
  tags, permissions, sealing, purecap/hybrid ABIs.
- `cheribsd-docs` — CheriBSD userspace, purecap, fault reporting.
- `cheribuild` — the build/run toolchain (`run-sdk`, `qemu-cheri`).

## Related skills

- `riscv-isa-and-rvv-intrinsics` — base RISC-V ISA this extension sits on.
- `c-string-and-buffer-safety` — classic buffer bugs CHERI catches.
- `c-undefined-behavior` — what remains UB under CHERI (out-of-bounds is now a
  trap, not UB).

## Evaluation

Synthetic: out-of-bounds derived pointer (`bad/bad_oob_derived.c`), byte-copy of
capability storage (`bad/bad_byte_copy.c`), integer round-trip tag loss
(`bad/bad_int_roundtrip.c`), missing StoreCap on a capability store
(`bad/bad_missing_perm.c`) — each must be flagged.
False-positive: a correct `cheri_bounds_set` sub-pointer within bounds
(`good/good_bounded.c`), a correct sealed-unseal round trip, and correct
`cheri_address_get`-based hashing must NOT be flagged.
Adversarial: an object-relative pointer that looks in-bounds but escapes the
object's bounds on the last element; and a pointer that passes review because it
never dereferences out-of-bounds in the tested path but loses the tag via
integer round-trip.
Commands and recorded results: `evals/README.md`.
