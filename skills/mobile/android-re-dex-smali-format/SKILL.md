---
name: android-re-dex-smali-format
description: Use when reading, writing, or reviewing smali and DEX-format artifacts: register model, instruction widths, verifier constraints, R8/ProGuard mapping parsing, and Kotlin-metadata name recovery. Prevents register-type confusion, move-object misuse, const width errors, and fabricated deobfuscation claims.
---

# Android RE: DEX/Smali Format and Deobfuscation

## When to use

- Reading or patching smali output (apktool/baksmali output).
- Auditing a decompiled app's bytecode for register/type/verifier errors.
- Parsing an R8/ProGuard `mapping.txt` to recover original names.
- Recovering Kotlin names via `@kotlin.Metadata` — or honestly concluding
  they are unrecoverable.
- Writing a smali patch (register moves, const changes, method calls).

## When not to use

- Native `.so` analysis — use `android-re-native-jni-analysis`.
- Static analysis of DEX-level call graphs at scale (use jadx/Ghidra for
  overview; this skill is for bytecode-level correctness).
- ARM/SoC-level reversing — different format entirely.
- Dynamic instrumentation of a running app — use
  `android-re-native-jni-analysis` (Frida).

## What the agent often gets wrong

- Treats smali registers as typed. The dex register model is untyped —
  registers hold 32-bit or 64-bit values — but the INSTRUCTION chosen must
  match the ART type: references need `move-object`/`iget-object`/`const-string`,
  longs need `move-wide`/`const-wide`/`add-long`. Mixing them is a verifier
  error at load time (the class is rejected or the register type tag is
  corrupted).
- Uses plain `move` where `move-object` is required (or vice versa). The
  two instruction families exist for a reason: `-object` moves
  references, plain moves are scalar 32-bit.
- Loads a 64-bit constant with `const` instead of `const-wide`, or returns
  a `long` with `return` instead of `return-wide`.
- Misparses `mapping.txt` direction: R8 writes `original -> obfuscated`.
  Reading it backwards is a very common, expensive error.
- Invents Kotlin names when metadata is absent. R8 strips Kotlin metadata
  in release builds (unless keep rules retain it); when the
  `@Lkotlin/Metadata` annotation is missing, original names are NOT
  recoverable from the dex — claiming them is fabrication.
- Assumes jadx output equals bytecode. jadx is a decompiler; its output is
  a reconstruction. For register/type-level facts, read the smali/dex, not
  the decompiled Java.

## How to reason correctly

1. **Classify every instruction by type family**: `-object` (references),
   `-wide` (64-bit scalars), plain (32-bit scalars). The register is the
   same bucket; the instruction determines the type interpretation.
2. **Match the return/load/store form to the declared type**: `I`/`Z` →
   plain, `J` → wide, reference → object. A mismatch fails verification.
3. **For deobfuscation, follow the evidence chain**: `mapping.txt` maps
   obfuscated names back to originals — read it left-to-right
   (original -> obfuscated, reverse to recover). Kotlin names come ONLY
   from the metadata annotation `data1`/`d1` when it survives R8.
4. **When metadata/mapping is missing, mark names INFERRED or UNKNOWN** —
   never fabricate. State exactly which artifact is missing.
5. **Verify with a disassembler/decompiler round-trip** (`baksmali d` /
   `jadx`) when available: the tool must reproduce the smali you claim to
   be reading.

## What to verify

- Every `move`/`const`/`return` matches the type family of the value it
  handles (object/wide/scalar).
- The class passes `baksmali` disassembly and (where tooling is present)
  the Android verifier (`dexdump`/`adb shell` on a device).
- `mapping.txt` was parsed in the correct direction; recovered names are
  traceable to a specific line.
- Kotlin names claimed are traceable to an actual `@kotlin.Metadata`
  annotation or a mapping entry — or marked unrecoverable.
- No claim about jadx output is presented as bytecode fact.

## How to verify

```
# Target flow (toolchain not installed on this host):
baksmali d classes.dex -o out_smali/
smali a out_smali/ -o reassembled.dex
# gate: reassembly must succeed (exit 0) — verifier-consistent smali.
jadx -d out app.apk          # decompiled view, for cross-check only
apktool d app.apk -s         # smali + resources (no dex->java)

# R8 mapping recovery (runnable anywhere, tool-free):
#   reverse the arrow direction:  obfuscated.a -> com.example.PaymentManager
#   then find the member line for the original method.
```

Researched — `jadx`/`apktool`/`baksmali` not installed on this Windows
host. Commands above are the documented verification; marked UNVERIFIED as
runs.

## Where the knowledge comes from

- `dex-spec` — DEX format: instruction set, register model, type
  descriptors, verifier constraints.
- `smali-docs` — smali syntax: register forms, move/const/return families,
  method descriptors.
- `art-docs` — how ART loads and verifies DEX bytecode (verifier rules the
  instruction families must obey).
- `jadx-docs` — jadx CLI and the decompiled-vs-bytecode distinction.
- `android-ndk` — cross-reference for native boundaries (see
  `android-re-native-jni-analysis`).

## Related skills

- `android-re-native-jni-analysis` — the native half of an APK; JNI
  signatures bridge from smali calls.
- `meta-verification-harness-validity` — "jadx shows X" is a harness
  result; verify the tool output is actually sensitive to the bytecode.
- `binary-analysis-type-recovery` — type recovery in native binaries
  (adjacent problem, different format).

## Evaluation

- Synthetic: `bad/register_type_confusion.smali`,
  `bad/move_object_confusion.smali`, `bad/const_width_confusion.smali`
  must be flagged; `good/correct_register_use.smali`,
  `good/wide_ops.smali` must NOT be.
- False-positive: `good/kotlin_metadata_kept.smali` — a real metadata
  annotation with recoverable names is not "guessing".
- Adversarial: `bad/mapping_misread.txt` (direction reversed, plausible
  format) and `bad/kotlin_metadata_guess.smali` (fabricated names) look
  like confident analysis.
- Historical: verifier-rejection classes documented in dex-spec/art-docs
  (UNVERIFIED as named incidents on this host).
- Researched commands and status: `evals/README.md`.
