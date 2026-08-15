# Android RE: DEX/Smali Format and Deobfuscation — Reference Rules

Knowledge layer for `android-re-dex-smali-format`. Format: RULE → WHY AI
GETS IT WRONG → CORRECT REASONING → EXAMPLE (bad) → COUNTEREXAMPLE (good)
→ VERIFICATION → SOURCE. Uncertainty marked KNOWN / INFERRED / UNVERIFIED.

The jadx/apktool/baksmali toolchain is NOT installed on this host; commands
are documented and marked UNVERIFIED as runs. DEX/smali semantics are KNOWN
from dex-spec, smali-docs, art-docs. Relative paths assume the skill
directory as CWD.

## 1. Smali registers are untyped; the instruction picks the interpretation

- **RULE**: in the dex register model, a register holds a 32-bit scalar, a
  64-bit wide value (a pair), or a reference, depending on the instruction
  that writes/reads it. There are no declared register types in smali —
  only method descriptors and instruction families imply types. The
  verifier tracks per-register type categories (reference, int, long,
  float, double) and rejects conflicting uses.
- **WHY AI GETS IT WRONG**: agents read a smali method and assign C-like
  types to registers from their names or from jadx output, then write
  instructions that mismatch the ART type categories.
- **CORRECT REASONING**: every instruction has an implied type family:
  `move`/`add-int`/`const` → 32-bit scalar; `move-wide`/`add-long`/
  `const-wide` → 64-bit; `-object` variants → reference. The verifier's
  register-category map is the ground truth.
- **EXAMPLE** (bad): `examples/bad/register_type_confusion.smali` — the
  int parameter `p0` is passed to `String.equals` and the boolean result is
  `int-to-long`'d.
- **COUNTEREXAMPLE** (good): `examples/good/correct_register_use.smali` —
  `iget-object` for the String field, `iget` for the int field,
  `move-result-object` for the StringBuilder result.
- **VERIFICATION**: reassembly (`smali a out/`) and loading on ART fail
  for the bad file and succeed for the good file. Commands documented;
  UNVERIFIED as runs (smali toolchain absent).
- **SOURCE**: dex-spec (register model, instruction set); smali-docs;
  art-docs (verifier).

## 2. move vs move-object vs move-wide are distinct families

- **RULE**: `move` moves a 32-bit scalar; `move-wide` moves a 64-bit
  value (both registers of a pair); `move-object` moves a reference.
  `move-result`, `move-result-wide`, `move-result-object` mirror this for
  invoke results. `iget`/`iget-wide`/`iget-object` do the same for field
  loads. Mixing families is a verifier error or a silent type-tag
  corruption.
- **WHY AI GETS IT WRONG**: the three look interchangeable in a diff and
  agents normalize them to "a move", especially when patching decompiled
  code where jadx already re-derived the types.
- **CORRECT REASONING**: the family must match the value kind the register
  will hold downstream. A String field is loaded with `iget-object` and
  copied with `move-object`; a `long` field with `iget-wide` and
  `move-wide`.
- **EXAMPLE** (bad): `examples/bad/move_object_confusion.smali` — a
  `const-string` reference is `move`'d (scalar) into v1 and stored with
  `iput-object`.
- **COUNTEREXAMPLE** (good): `examples/good/correct_register_use.smali`
  lines 24-28 — `move-result-object v0` after `toString()`.
- **VERIFICATION**: ART class-loading rejects the bad file (verifier);
  reassembly of the good file is clean. UNVERIFIED as runs; the family
  rules are KNOWN.
- **SOURCE**: dex-spec (move families); smali-docs; art-docs (verifier).

## 3. const / const-wide / const-string and the 64-bit forms

- **RULE**: `const vN, lit32` loads a signed 32-bit constant; `const-wide
  vN, lit64` loads a 64-bit constant into a register pair; `const-string`
  and `const-class` handle references. A 64-bit value loaded with `const`
  is a verifier type conflict, and `return` of a `long` is invalid
  (`return-wide` is required).
- **WHY AI GETS IT WRONG**: agents write `const v0, 0x1000000000` "because
  the value is big" and use plain `move`/`return` for longs, ignoring the
  wide category.
- **CORRECT REASONING**: the width is determined by the declared type (J →
  wide pair), not by the magnitude. Every use of the register (move,
  arithmetic, return) must be in the same family.
- **EXAMPLE** (bad): `examples/bad/const_width_confusion.smali` — `const
  v0, 0x1000000000` (out of int range, wrong form), `move v1, v0` (should
  be `move-wide`), `return-wide v0` (pair not established).
- **COUNTEREXAMPLE** (good): `examples/good/wide_ops.smali` — `const-wide
  v0, 0x1000000000`, `move-wide v1, v0`, `add-long`, `return-wide`.
- **VERIFICATION**: verifier rejects the bad file; the good file
  assembles and runs. UNVERIFIED as runs; the width rules are KNOWN.
- **SOURCE**: dex-spec (const forms); smali-docs.

## 4. R8/ProGuard mapping.txt: read original -> obfuscated, reverse to recover

- **RULE**: `mapping.txt` (and `apk-mapping.txt`) format: class line
  `original -> obfuscated:`, member lines
  `[range] [access]:[access]:original.member:line -> obfuscated`. To
  recover original names, reverse the arrow: the right side is what is in
  the dex, the left side is the original source name. R8's `-printmapping`
  writes this; R8 also strips Kotlin metadata unless kept (see rule 5).
- **WHY AI GETS IT WRONG**: agents read the mapping as "obfuscated ->
  original" (the natural reading of a file whose purpose is deobfuscation)
  and report the wrong names, or they treat the presence of a mapping file
  as proof every name is recoverable.
- **CORRECT REASONING**: the file is generated by the obfuscator to
  document what it DID; it lists original names first. Recovery is
  original = left of the arrow. When a class is not in the mapping at all,
  its name in the dex IS the original (no obfuscation applied) only if it
  was kept — verify against keep rules.
- **EXAMPLE** (bad): `examples/bad/mapping_misread.txt` — the comment
  claims `a -> CheckoutActivity` is the mapping direction, reversing it.
- **COUNTEREXAMPLE** (good): `examples/good/mapping_correct.txt` — arrow
  direction documented correctly with original methods on the left.
- **VERIFICATION**: pick a known method, find its obfuscated name in the
  dex (`jadx`/`baksmali`), and confirm the mapping line maps original ->
  obfuscated. UNVERIFIED as a tool run; the format is KNOWN.
- **SOURCE**: dex-spec (names in dex); smali-docs; art-docs; jadx-docs
  (cross-check).

## 5. Kotlin metadata: recoverable ONLY if @kotlin.Metadata survived R8

- **RULE**: Kotlin original names (parameter names, local functions,
  property backing) live in the `@Lkotlin/Metadata;` annotation's `data1`
  (`d1`) payload. R8 removes Kotlin metadata in release builds by default;
  it survives only when keep rules retain it (or in debug builds). If the
  annotation is absent, original Kotlin names are not recoverable from the
  dex — claiming them is fabrication.
- **WHY AI GETS IT WRONG**: agents "know" Kotlin names are in metadata and
  produce plausible-sounding names even when no metadata annotation exists,
  because the model cannot distinguish "recoverable" from "guessed".
- **CORRECT REASONING**: locate the annotation in the class's smali. If
  present, extract `data1` strings for names. If absent, state: "Kotlin
  metadata stripped by R8 — original parameter names unrecoverable".
  Never substitute a guess for a missing artifact.
- **EXAMPLE** (bad): `examples/bad/kotlin_metadata_guess.smali` — the
  comment fabricates `com.example.UserRepository` as the "original name"
  although the class has no metadata annotation.
- **COUNTEREXAMPLE** (good): `examples/good/kotlin_metadata_kept.smali` —
  the annotation is present with a `data1` payload, and the recovered name
  is traceable to it.
- **VERIFICATION**: grep the class smali for `Lkotlin/Metadata;`; the
  presence/absence decides recoverability. UNVERIFIED as a tool run; the
  mechanism is KNOWN.
- **SOURCE**: art-docs (annotations in dex); dex-spec (annotation payloads);
  smali-docs.

## 6. jadx output is a reconstruction, not bytecode

- **RULE**: jadx produces Java-like source reconstructed from bytecode.
  Types, control flow, and names it shows are inferred; the smali/dex is
  the authoritative artifact. Register-level and verifier-level facts
  (rule 1-3) must be read from the bytecode.
- **WHY AI GETS IT WRONG**: agents quote "jadx shows the method returns
  String" and then write smali that contradicts the bytecode, or trust
  jadx's reconstructed parameter names (which it invents when metadata is
  stripped — see rule 5).
- **CORRECT REASONING**: use jadx for orientation and for mapping recovered
  names onto behavior, but verify any type/register claim against the
  actual instructions. If jadx and smali disagree, the smali is the fact.
- **EXAMPLE** (bad): trusting jadx's parameter naming for a
  metadata-stripped class and reporting those names as original.
- **COUNTEREXAMPLE** (good): `examples/good/kotlin_metadata_kept.smali` —
  names traced to the annotation; jadx used only as orientation.
- **VERIFICATION**: run both `jadx -d out app.apk` and
  `apktool d app.apk -s`; the smali view is the base for bytecode claims.
  UNVERIFIED as runs; the tool distinction is KNOWN.
- **SOURCE**: jadx-docs (CLI, reconstruction semantics); dex-spec.

## Quick reference table

| Topic | Rule in one line |
|---|---|
| register model | untyped buckets; instruction family sets the type category |
| move families | -object for refs, -wide for 64-bit, plain for 32-bit |
| const forms | const lit32, const-wide lit64, const-string ref |
| returns | return, return-wide, return-object per declared type |
| mapping.txt | original -> obfuscated; reverse to recover |
| Kotlin names | only from surviving @kotlin.Metadata data1; else unrecoverable |
| jadx | reconstruction, not bytecode fact |
