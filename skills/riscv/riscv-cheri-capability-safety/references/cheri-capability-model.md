# CHERI Capability Safety — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to `registry/sources.yaml`.

## 1. A capability has four properties: address, bounds, permissions, tag

- **RULE**: a CHERI capability is (address, bounds, permissions, tag). A memory
  access faults if the address falls outside the bounds, if a required permission
  is absent, or if the tag is clear — even when the address bytes look valid.
- **WHY AI GETS IT WRONG**: models CHERI as "fat pointer" only; checks the address
  range but ignores permissions/tag; treats a tag-clear pointer as a normal
  (non-capability) value.
- **CORRECT REASONING**: these are four independent guards. Fix each bug class
  differently: extend bounds (`cheri_bounds_set`), add a permission
  (`cheri_perms_and` mask or the object allocation), or restore the tag (re-derive
  from a tagged source — you cannot "untag-clear" by writing). A dereference of a
  tag-clear pointer is a hardware fault, not UB.
- **EXAMPLE** (bad): `*(int*)p` where `p` is the result of
  `(intptr_t)p` round-trip — tag cleared, fault on access.
- **COUNTEREXAMPLE** (good): keep the capability as a pointer; derive sub-pointers
  with `cheri_offset_set`/`cheri_bounds_set`, never through an integer.
- **VERIFICATION**: on QEMU-CHERI the bad case faults with a tag error
  (CheriBSD: `SIGPROT`, `si_code` CHERI tag); documented, not run here. Python
  model in `examples/good/sim_cheri_model.py` checks bounds/permission/tag
  enforcement logic.
- **SOURCE**: `cheri-spec` §2 (capability model), §6.4 (bounds), §6.5 (permissions),
  §6.7 (tags); `cheribsd-docs` (fault reporting).

## 2. Pointer arithmetic must stay within bounds

- **RULE**: arithmetic that carries the address outside the capability bounds
  clears the tag. The result is a tag-clear pointer even if the final address
  would (numerically) be inside another valid object.
- **WHY AI GETS IT WRONG**: reasons about the numeric address only ("0x1000+8 =
  0x1008, inside the heap"); or assumes the tag is kept as long as the final
  address is in-bounds.
- **CORRECT REASONING**: bounds are checked *monotonically* during derivation:
  `p + k` is valid only if every intermediate address (or the whole walk per the
  CHERI derivation rules) stays in bounds. Use `cheri_bounds_set` first, then
  offsets within it; any out-of-bounds step destroys the capability.
- **EXAMPLE** (bad): `int *q = p + (n + 1);` — derived beyond the object.
- **COUNTEREXAMPLE** (good): `int *q = p + n;` with `n ≤ object_size` verified;
  or `q = cheri_offset_set(p, n*sizeof(int))` after bounds are set.
- **VERIFICATION**: QEMU-CHERI faults on the bad arithmetic; documented.
- **SOURCE**: `cheri-spec` §6.4 (bounds and pointer arithmetic), §6.7 (tags).

## 3. Byte-wise copies of capability storage clear the tag

- **RULE**: copying a capability's raw bytes (memcpy, byte loop, casting to a byte
  array) drops the tag. A later dereference of the copy faults. Capability storage
  must be copied with capability-aware operations (or whole registers).
- **WHY AI GETS IT WRONG**: uses `memcpy`/`memmove` on arrays of pointers in
  purecap; or serializes a pointer to bytes for RPC and "restores" it later.
- **CORRECT REASONING**: memcpy copies the payload, not the hardware tag bit
  (tags live beside the data, not inside it). For an array of capabilities, use
  element-wise copy of the pointers, or copy via capability stores. For RPC,
  transfer an *authority token* (e.g. shared-memory capability or fd), never raw
  bytes.
- **EXAMPLE** (bad): `memcpy(dst, src, N * sizeof(void*));` in purecap — tags lost.
- **COUNTEREXAMPLE** (good):
  ```c
  for (size_t i = 0; i < N; i++) dst[i] = src[i];   // element-wise cap copy
  ```
- **VERIFICATION**: fault on dereference of the memcpy'd pointer; documented.
- **SOURCE**: `cheri-spec` §6.7 (tags), §6.10 (C ABI rules for capabilities).

## 4. Purecap vs hybrid ABI is a real ABI difference

- **RULE**: purecap makes every pointer a capability (larger pointers, capability
  calling convention, capability-aware alignment). Hybrid ABI keeps a separate
  non-capability address space, so code must distinguish capability vs
  non-capability pointers explicitly.
- **WHY AI GETS IT WRONG**: assumes the same source works in both ABIs; writes
  `sizeof(void*)`-dependent layout or assumes pointer size is 8 bytes; forgets
  `va_list`/variadic differences.
- **CORRECT REASONING**: purecap pointers are (on Morello) 16 bytes; struct
  layout, alignment, and the calling convention change. Choose the ABI for the
  whole program and mark the few hybrid entry points with the explicit
  capability/non-capability annotations; do not mix pointer widths silently.
- **EXAMPLE** (bad): hard-coded `8` for pointer size in an allocator used with a
  purecap toolchain.
- **COUNTEREXAMPLE** (good): use `sizeof(void*)` everywhere; compile once as
  purecap and once hybrid to catch layout assumptions.
- **VERIFICATION**: compile the same source with `purecap-cc` and `hybrid-cc`;
  compare object layout. Documented.
- **SOURCE**: `cheri-spec` §6.9-6.11 (purecap/hybrid C ABI); `cheribsd-docs`
  (purecap userspace).

## 5. Sealing protects object authority

- **RULE**: sealing a capability restricts it to be usable only as a key or as an
  object passed to code holding the matching `unseal` key. A sealed capability
  cannot be dereferenced or modified until unsealed with the correct key.
- **WHY AI GETS IT WRONG**: treats sealing as "another permission bit"; or assumes
  any unseal works; or passes sealed capabilities to unrelated components.
- **CORRECT REASONING**: seal/unseal is a paired authority: the sealer (or an
  agreed key) is the only one who can unseal. Use sealing to hand a "token"
  without granting dereference rights; unseal only at the point that owns the key.
- **EXAMPLE** (bad): unsealing a capability with a different key than it was
  sealed with — faults.
- **COUNTEREXAMPLE** (good): seal with key K, pass the sealed cap, unseal with K
  at the authorized consumer.
- **VERIFICATION**: fault on mismatched unseal; documented.
- **SOURCE**: `cheri-spec` §6.8 (sealing).

## 6. Bounded sub-pointers are the intended pattern (FP guard)

- **RULE**: creating a sub-capability with `cheri_bounds_set(p, size)` that covers
  exactly the sub-object is correct and is the recommended CHERI style. It must
  NOT be flagged as "unsafe pointer narrowing" merely because the bounds are
  narrower than the whole allocation.
- **WHY AI GETS IT WRONG**: treats any bounded pointer as a "weird cast"; or
  recommends re-widening bounds "to be safe", which defeats CHERI.
- **CORRECT REASONING**: the whole point of CHERI is per-object bounds. A
  sub-pointer bounded to the object it may access is the safe form. Only flag a
  *bounds misconfig*: length larger than the object, or an offset such that the
  accessed range escapes.
- **EXAMPLE** (bad review): flagging `cheri_bounds_set(ptr, 64)` on a 64-byte
  object as "unsafe".
- **COUNTEREXAMPLE** (good review): accept it; flag `cheri_bounds_set(ptr, 128)`
  on the same 64-byte object.
- **VERIFICATION**: model checks that the good sub-pointer stays in-bounds; the
  over-wide one does not.
- **SOURCE**: `cheri-spec` §6.4 (bounds_set), §6.10 (C idioms).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Model | address + bounds + permissions + tag; any miss ⇒ fault |
| Arithmetic | escaping bounds clears the tag; set bounds, then offset |
| Byte copy | memcpy of caps drops tags; copy elements, not bytes |
| ABI | purecap ≠ hybrid: pointer size, layout, calling convention differ |
| Sealing | seal/unseal is a paired key; wrong key faults |
| Bounded subs | correct pattern — flag only bounds OVER the object |
| Integer cast | `(uintptr_t)` round-trip clears the tag; use `cheri_address_get` |
| Permissions | LoadCap/StoreCap required for capability load/store |
