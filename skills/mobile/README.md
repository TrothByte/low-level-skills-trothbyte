# mobile — Skills

Android reverse engineering requires understanding two runtime layers.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `android-re-dex-smali-format` | Use when reading, writing, or reviewing smali and DEX-format artifacts: register model, instruction widths, verifier constraints, R8/ProGuard mapping parsing, and Kotlin-metadata name recovery. Prevents register-type confusion, move-object misuse, const width errors, and fabricated deobfuscation claims. | unique | researched | `skills/mobile/android-re-dex-smali-format` |
| `android-re-native-jni-analysis` | Use when analyzing native .so code in Android apps: JNI symbol names/signatures, JNIEnv function-table access, reference/array hygiene, and dynamic analysis via Frida. Prevents UnsatisfiedLinkError-style signature errors, global-ref leaks, and hooking guessed addresses instead of fingerprint-first real ones. | unique | researched | `skills/mobile/android-re-native-jni-analysis` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
  (`references/` hold deep knowledge; `examples/good` and `examples/bad` are compiled/run
  fixtures; `evals/README.md` defines eval cases.)
- Load only the skill you need (see `skills/_meta/meta-routing`; references load on demand.

## Related

[Back to repository root](../../README.md)
