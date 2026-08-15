# Evaluation — android-re-native-jni-analysis

Skill: `skills/mobile/android-re-native-jni-analysis`. Stability target:
`evaluated`. JNI semantics KNOWN from jni-spec/android-ndk. Toolchain
(frida + Android target) NOT available on this host — tool runs marked
UNVERIFIED.

## Synthetic evals

| Case | Fixture | Expected | Status |
|---|---|---|---|
| easy/negative | `bad/jni_signature_mismatch.c` | wrong symbol name/params flagged | structural |
| medium/negative | `bad/jni_global_ref_leak.c` | NewGlobalRef without Delete flagged | structural |
| medium/negative | `bad/jni_array_release_missing.c` | Get/Release imbalance flagged | structural |
| easy/positive | `good/jni_correct_signature.c` | exact symbol + full params accepted | structural |
| medium/positive | `good/jni_correct_references.c` | balanced refs, NULL checks, Release accepted | structural |
| medium/positive | `good/frida_fingerprint.js` | fingerprint-first hook script accepted | structural |

Detection rule: derive the expected symbol from the Java declaration and
diff against the C function; count NewGlobalRef vs DeleteGlobalRef and
Get*ArrayElements vs Release*ArrayElements; require NULL checks on
object-returning JNI calls.

## False-positive evals (correct code must NOT be flagged)

- `good/jni_correct_signature.c` — exact name and parameter list.
- `good/jni_correct_references.c` — local-ref-only function with NULL
  checks is NOT a leak; global ref created once and released in
  JNI_OnUnload is balanced.
- `RegisterNatives`-based binding (no `Java_...` export) is a valid
  pattern, not a signature error — the hook target is the registration
  call, not a guessed export.

## Historical evals

- JNI global-reference overflow aborts and UnsatisfiedLinkError from
  signature mismatches are documented behaviors in jni-spec and
  android-ndk. UNVERIFIED as named incidents on this host (no device,
  no crash database).

## Adversarial evals

- `bad/jni_signature_mismatch.c`: the library compiles and loads; the
  failure appears only at the first Java call (UnsatisfiedLinkError). A
  unit review that compiles the C and stops is not enough — the Java
  declaration must be in scope.
- `bad/jni_array_release_missing.c`: returns a correct-looking sum; the
  leak is invisible in a short run and only manifests as pinned-memory
  exhaustion under load.
- Stripped `.so`: an agent that hooks a guessed export reports "no calls"
  instead of re-fingerprinting — the fingerprint-first rule (rule 5) and
  hit-counting (rule 6) are the only defenses. Cross-referenced with
  `meta-verification-harness-validity`.

## Verification commands (RESEARCHED, toolchain not available)

```
frida -U -l examples/good/frida_fingerprint.js com.example.app
objdump -T libnative-lib.so | grep Java_com_
nm -D libnative-lib.so | grep JNI_OnLoad
```

frida not installed and no Android target attached (Windows MSYS2 host).
objdump exists locally but analyzes PE targets; Android ELF `.so` requires
the NDK/toolchain. Commands documented as the target verification flow
(frida-docs). The three bad C files carry the `// intentionally incorrect`
marker.

## Verified facts

- KNOWN: exact JNI symbol naming rules; JNIEnv function-table access;
  local/global/weak-global reference lifecycle; Get/Release array pairs;
  ART global-ref limits. Sources: jni-spec, android-ndk.
- KNOWN: Frida Module/Process API shape (findModuleByName,
  enumerateExports) from frida-docs.
- UNVERIFIED (toolchain absent): actual frida/objdump runs on an Android
  target; on-device JNI global-ref overflow reproduction.

## Scoring

- precision: every flagged issue maps to a reference rule (1-6).
- recall: all three bad fixtures detected (signature, global refs, array
  release).
- FP-rate: the three good fixtures produce zero flags.
- Decisive test: "if the Java declaration changed, would the C symbol
  change?" and "is every reference/array API paired and NULL-checked?"
