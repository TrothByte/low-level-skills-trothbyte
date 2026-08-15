# mobile — Skills

Low-level engineering skills for this domain.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `android-re-dex-smali-format` | Use when reading, writing, or reviewing smali and DEX-format artifacts: register model, instruction widths, verifier constraints, R8/ProGuard mapping parsing, and Kotlin-metadata name recovery. Prevents register-type confusion, move-object misuse, const width errors, and fabricated deobfuscation claims. | researched | `skills/mobile/android-re-dex-smali-format` |
| `android-re-native-jni-analysis` | Use when analyzing native .so code in Android apps: JNI symbol names/signatures, JNIEnv function-table access, reference/array hygiene, and dynamic analysis via Frida. Prevents UnsatisfiedLinkError-style signature errors, global-ref leaks, and hooking guessed addresses instead of fingerprint-first real ones. | researched | `skills/mobile/android-re-native-jni-analysis` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
