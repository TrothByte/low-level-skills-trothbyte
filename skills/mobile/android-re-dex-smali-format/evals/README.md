# Evaluation — android-re-dex-smali-format

Skill: `skills/mobile/android-re-dex-smali-format`. Stability target:
`evaluated`. DEX/smali semantics KNOWN from dex-spec/smali-docs/art-docs.
Toolchain (jadx/apktool/baksmali) NOT installed on this host — tool runs
marked UNVERIFIED.

## Synthetic evals

| Case | Fixture | Expected | Status |
|---|---|---|---|
| easy/negative | `bad/register_type_confusion.smali` | verifier type conflict flagged | structural |
| easy/negative | `bad/move_object_confusion.smali` | move vs move-object misuse flagged | structural |
| medium/negative | `bad/const_width_confusion.smali` | const/wide/return-width mismatch flagged | structural |
| medium/negative | `bad/mapping_misread.txt` | reversed arrow direction flagged | structural |
| medium/negative | `bad/kotlin_metadata_guess.smali` | fabricated names without metadata flagged | structural |
| medium/positive | `good/correct_register_use.smali` | correct object/scalar usage accepted | structural |
| easy/positive | `good/wide_ops.smali` | consistent wide pairs accepted | structural |
| easy/positive | `good/mapping_correct.txt` | arrow direction correct | structural |
| medium/positive | `good/kotlin_metadata_kept.smali` | names traced to real annotation | structural |

Detection rule: for each instruction, name the type family it implies
(object/wide/scalar) and check every register it touches is used
consistently across the method. For deobfuscation claims, trace each name
to a mapping line or a metadata annotation — untraceable names are
fabricated.

## False-positive evals (correct code must NOT be flagged)

- `good/correct_register_use.smali` — object fields/methods with -object
  forms, int field with `iget`, constructor properly direct-invoked.
- `good/wide_ops.smali` — longs use const-wide/move-wide/add-long/
  return-wide consistently.
- `good/kotlin_metadata_kept.smali` — a present annotation with a data1
  payload is legitimate recovery, not a guess.
- A class that was never obfuscated (not in mapping.txt) is not a mapping
  error.

## Historical evals

- Verifier-rejection classes and register-family violations are documented
  behavior in dex-spec (instruction set) and art-docs (verifier). No
  named incident/CVE dataset on this host — UNVERIFIED as specific cases.
- R8 metadata stripping in release builds is documented R8/ProGuard
  behavior (art-docs) — KNOWN mechanism, no tool run.

## Adversarial evals

- `bad/mapping_misread.txt` looks like a valid mapping file; the bug is
  the claimed direction. An agent that "reads the mapping" without
  checking the arrow direction produces confident wrong names.
- `bad/kotlin_metadata_guess.smali` produces plausible original names with
  no artifact to back them — the exact failure mode of models recovering
  names from an empty metadata store. The rule "no annotation -> no names"
  is the only defense.
- `bad/register_type_confusion.smali` compiles-looking smali that the ART
  verifier rejects at load — "it looks like valid bytecode" is not enough;
  the verifier's register categories are the ground truth.

## Verification commands (RESEARCHED, toolchain not available)

```
baksmali d classes.dex -o out_smali/
smali a out_smali/ -o reassembled.dex
jadx -d out app.apk
apktool d app.apk -s
```

jadx/apktool/baksmali not installed (Windows MSYS2). The commands are the
documented verification flow (jadx-docs, smali-docs). The three bad smali
files, the bad mapping file, and the bad metadata file carry the
`# intentionally incorrect` marker.

## Verified facts

- KNOWN: untyped register model with instruction-family type categories;
  move/move-wide/move-object families; const/const-wide forms; mapping.txt
  direction (original -> obfuscated); Kotlin names require surviving
  metadata; jadx is a reconstruction. Sources: dex-spec, smali-docs,
  art-docs, jadx-docs.
- UNVERIFIED (toolchain absent): actual baksmali/smali/jadx runs on these
  fixtures.

## Scoring

- precision: every flagged instruction maps to a reference rule (1-6) and
  a type-family mismatch or untraceable-name claim.
- recall: all five bad fixtures detected (register/type, move family, wide
  forms, mapping direction, fabricated metadata names).
- FP-rate: the four good fixtures produce zero flags.
- Decisive test: "can every claimed original name be traced to a mapping
  line or metadata payload?" and "do all instructions touching this
  register share a type family?"
