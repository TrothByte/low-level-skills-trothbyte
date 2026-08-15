---
name: android-re-native-jni-analysis
description: Use when analyzing native .so code in Android apps: JNI symbol names/signatures, JNIEnv function-table access, reference/array hygiene, and dynamic analysis via Frida. Prevents UnsatisfiedLinkError-style signature errors, global-ref leaks, and hooking guessed addresses instead of fingerprint-first real ones.
---

# Android RE: Native JNI Analysis

## When to use

- Reverse engineering or auditing native code in an Android APK (`.so`
  files loaded via `System.loadLibrary`).
- Reading JNI exports (`Java_com_...` symbols) and `RegisterNatives`
  registrations.
- Hooking native functions with Frida — fingerprint the module first.
- Reviewing C/C++ JNI code for signature, reference, or array-API errors.

## When not to use

- DEX/smali layer analysis — use `android-re-dex-smali-format`.
- Analyzing the `.so` instruction set itself (ARM/Thumb disassembly
  details) — use `binary-analysis-type-recovery`/`asm-*` skills.
- Non-Android native code (desktop Linux/Windows) — JNIEnv semantics
  differ; the JNI spec still applies but the Android runtime specifics do
  not.

## What the agent often gets wrong

- Wrong JNI symbol signature: a native method declared `native long
  addInts(int a, int b)` requires `Java_com_example_Main_addInts(JNIEnv*,
  jobject, jint, jint)`. Missing the `JNIEnv*`/`jobject` parameters, or
  getting the package/class name wrong in the symbol, causes
  `UnsatisfiedLinkError` at runtime.
- Uses `(void)` or made-up parameter lists that match neither the Java
  declaration nor the JNI spec, and "fixes" the mismatch by changing the
  Java side or the library load — the real error is the symbol/signature.
- Forgets the `JNIEnv*` is a pointer to a function table: every call is
  `(*env)->Function(env, ...)`, and `env` itself must be passed as the
  first argument.
- Leaks global references: `NewGlobalRef` results must be released with
  `DeleteGlobalRef`; local refs are auto-released at native-frame exit but
  must be NULL-checked. In an Android app, leaked global refs crash ART
  with JNI Global Reference overflow.
- Calls `GetIntArrayElements` without a matching `ReleaseIntArrayElements`
  — the array stays pinned and can exhaust memory, and the native writes
  are never committed back.
- Hooks Frida on guessed addresses/exports without fingerprinting the
  module first — symbols may be stripped, code may be packed/obfuscated,
  and the hook silently misses.

## How to reason correctly

1. **Fingerprint first**: enumerate the module (`Process.findModuleByName`)
  and its exports before hooking. For stripped `.so` files, find
  `JNI_OnLoad` and hook `RegisterNatives` to discover dynamically
  registered methods.
2. **Derive the expected symbol from the Java declaration**: package +
  class + method (underscored, mangled for special chars), plus the two
  implicit parameters (`JNIEnv*`, `jobject` or `jclass`), then the
  declared parameters mapped to JNI types (`jint`, `jlong`, `jstring`,
  `jobject`, ...).
3. **Check JNIEnv access**: every JNI call goes through the function table:
  `(*env)->FindClass(env, ...)`, `(*env)->GetIntArrayElements(env, ...)`.
  Missing `env` or using the table without the env arg is a crash.
4. **Audit reference lifetime**: global refs need explicit release;
  local refs auto-release but need NULL checks; array element pinning
  needs Release after use.
5. **Verify symbol existence** with `nm -D`/`objdump -T` on the `.so`:
  if the symbol is absent (stripped or `RegisterNatives`-based), hook the
  registration path instead of the (nonexistent) export.

## What to verify

- The native symbol name matches the Java declaration exactly
  (package/class/method/params), including the implicit JNIEnv* and
  jobject.
- Every JNI function call passes `env` as its first argument through the
  function table.
- Global refs are balanced by DeleteGlobalRef; arrays are released; all
  object-returning calls are NULL-checked.
- Frida hooks target addresses confirmed by fingerprint (module base +
  real exports or RegisterNatives registration), not guessed names.
- The `.so` exports are enumerated (`objdump -T` / Frida) and recorded.

## How to verify

```
# Target flow (frida + objdump documented; toolchain not installed here):
frida -U -l examples/good/frida_fingerprint.js com.example.app
#   prints module base/size + export table; hooks go on real addresses.

objdump -T libnative-lib.so | grep Java_com_        # symbol presence
nm -D libnative-lib.so | grep JNI_OnLoad             # entry point

# For stripped libs: hook RegisterNatives in JNI_OnLoad via Frida.
```

Researched — `frida` and `objdump`-for-ELF not verified on this Windows
host (objdump is present for PE targets; Android ELF .so analysis requires
the NDK toolchain). Commands documented; marked UNVERIFIED as runs on an
Android target.

## Where the knowledge comes from

- `jni-spec` — JNI function table, symbol naming, reference categories
  (local/global/weak global), array pinning APIs.
- `android-ndk` — NDK JNI usage on Android, ART-specific reference
  behavior and JNI limits.
- `frida-docs` — `Process.findModuleByName`, `Module.enumerateExports`,
  `Interceptor.attach`, `RegisterNatives` hooking.

## Related skills

- `android-re-dex-smali-format` — the Java/dex side: JNI declarations and
  `System.loadLibrary` call sites.
- `binary-analysis-type-recovery` — type recovery for stripped native
  binaries.
- `meta-verification-harness-validity` — "the hook ran" is a harness
  result; verify the hook actually fired (fingerprint-first is the
  ablation for it).
- `elf-linker-loader-debugger` — `.so` loading, symbol tables,
  relocation (relevant when inspecting the module programmatically).

## Evaluation

- Synthetic: `bad/jni_signature_mismatch.c`,
  `bad/jni_global_ref_leak.c`, `bad/jni_array_release_missing.c` must be
  flagged; `good/jni_correct_signature.c`,
  `good/jni_correct_references.c`, `good/frida_fingerprint.js` must NOT
  be.
- False-positive: a correct local-ref-only function with NULL checks is
  not a leak; a `RegisterNatives`-based binding is a valid pattern, not a
  signature error.
- Adversarial: a stripped `.so` where the agent hooks a guessed export
  that never fires — fingerprint-first is the only defense.
- Historical: JNI global-ref overflow crashes and UnsatisfiedLinkError
  cases documented in jni-spec/android-ndk (UNVERIFIED as named incidents
  on this host).
- Researched commands and status: `evals/README.md`.
