# Android RE: Native JNI Analysis — Reference Rules

Knowledge layer for `android-re-native-jni-analysis`. Format: RULE → WHY AI
GETS IT WRONG → CORRECT REASONING → EXAMPLE (bad) → COUNTEREXAMPLE (good)
→ VERIFICATION → SOURCE. Uncertainty marked KNOWN / INFERRED / UNVERIFIED.

The frida toolchain and an Android target are NOT available on this host;
commands are documented and marked UNVERIFIED as runs. JNI semantics are
KNOWN from jni-spec, android-ndk. Relative paths assume the skill directory
as CWD.

## 1. The native symbol must encode the exact Java signature

- **RULE**: for an implicit-binding native method, the exported C function
  is `Java_<mangled-package>_<mangled-class>_<method>(JNIEnv*, jobject|
  jclass, ...params)`. Package dots become underscores; class/method names
  are mangled per the JNI spec for special characters; the first two
  parameters are the JNIEnv pointer and the `this`/class reference. The
  declared Java return type maps to the JNI return type (`jlong` for
  `long`, `jobject` for a reference, etc.). A mismatch produces
  `UnsatisfiedLinkError`.
- **WHY AI GETS IT WRONG**: agents write the C function with a made-up
  `(void)` or partial parameter list, or miss the implicit JNIEnv/jobject,
  because the symbol name is "long and mechanical" and looks reducible.
- **CORRECT REASONING**: derive the symbol mechanically from the Java
  declaration: `com.example.Main.addInts` → `Java_com_example_Main_addInts`
  (dot→underscore), parameters `(JNIEnv*, jobject, jint, jint)` → `jlong`
  return.
- **EXAMPLE** (bad): `examples/bad/jni_signature_mismatch.c` — a `(void)`
  function named `Java_com_example_Main_addInts` (wrong return and no
  params) plus a `com_example2` name mismatch.
- **COUNTEREXAMPLE** (good): `examples/good/jni_correct_signature.c` —
  `JNIEXPORT jlong JNICALL Java_com_example_Main_addInts(JNIEnv*, jobject,
  jint, jint)`.
- **VERIFICATION**: `nm -D libnative-lib.so | grep Java_com_example_Main`
  must show the exact symbol; the Java `native` declaration and the C
  signature must agree type-for-type. UNVERIFIED as a run (no Android
  target); the naming rule is KNOWN.
- **SOURCE**: jni-spec (native method naming); android-ndk.

## 2. JNIEnv is a pointer to a function table

- **RULE**: `JNIEnv* env` points to a table of function pointers. Every
  JNI call is `(*env)->Function(env, ...)`, with `env` passed explicitly
  as the first argument. There is no direct call syntax and no global
  JNIEnv.
- **WHY AI GETS IT WRONG**: agents call JNI functions as if they were
  ordinary C functions (`FindClass(...)`), or use the table without the
  env argument — the classic "double-deref missing" crash.
- **CORRECT REASONING**: the table indirection and the env argument are
  mandatory; writing `(*env)->FindClass(env, "java/lang/String")` is the
  only correct form. When reading disassembly, a call through the env
  table is recognizable as an indirect call on a register loaded from the
  env pointer.
- **EXAMPLE** (bad): `examples/bad/jni_global_ref_leak.c` line 12 — the
  table access is correct but the env argument and reference lifecycle
  are handled wrongly (see rule 3).
- **COUNTEREXAMPLE** (good): `examples/good/jni_correct_references.c` —
  every call `(*env)->Function(env, ...)`.
- **VERIFICATION**: compile for a JNI target and observe linker errors for
  missing env; static review for the double-deref pattern. UNVERIFIED as
  a run here.
- **SOURCE**: jni-spec (JNIEnv function table); android-ndk.

## 3. Global references must be released; local refs auto-release but must be NULL-checked

- **RULE**: JNI has local, global, and weak-global references. Local refs
  are freed automatically when the native call returns (or with
  `DeleteLocalRef`); global refs (from `NewGlobalRef`) live until
  `DeleteGlobalRef`. Leaked global refs accumulate and, on ART, trigger
  a "JNI global reference overflow" abort. Object-returning calls must be
  NULL-checked before dereference.
- **WHY AI GETS IT WRONG**: agents treat `NewGlobalRef` as a "stronger
  reference" that also cleans itself, or skip NULL checks on `FindClass`/
  `NewStringUTF` because "it won't fail in practice".
- **CORRECT REASONING**: global refs are a resource with explicit
  lifetime; a native method that promotes a local ref to global without
  releasing it is a leak per call. NULL means an exception is pending —
  return immediately (the exception propagates to the caller).
- **EXAMPLE** (bad): `examples/bad/jni_global_ref_leak.c` — `NewGlobalRef`
  per call, never `DeleteGlobalRef`; `NewStringUTF` result not NULL-checked.
- **COUNTEREXAMPLE** (good): `examples/good/jni_correct_references.c` —
  NULL checks on `FindClass` and `NewStringUTF`; the global class ref is
  created once and released in `JNI_OnUnload`.
- **VERIFICATION**: on-device runs of the bad code eventually abort with
  JNI global ref overflow; static audit counts NewGlobalRef vs
  DeleteGlobalRef. UNVERIFIED as a run (no device); the rule is KNOWN.
- **SOURCE**: jni-spec (reference categories, global ref lifecycle);
  android-ndk (ART limits).

## 4. Array pinning: GetIntArrayElements requires ReleaseIntArrayElements

- **RULE**: `GetIntArrayElements` returns a pointer into the array (pinned
  or copied, mode flag dependent); the native code MUST call
  `ReleaseIntArrayElements(env, arr, elems, mode)` when done. Omitting it
  leaks the pin (and, for large arrays, exhausts memory); omitting the
  final write-back mode leaves modifications uncommitted.
- **WHY AI GETS IT WRONG**: the element pointer behaves like a plain C
  buffer in training data; agents pattern-match to "malloc-free" pairs and
  forget the JNI pair entirely.
- **CORRECT REASONING**: the pair is (Get, Release) — check both are
  present and that Release is on every return path. When reading
  disassembly, the Release call is a JNI table call taking the array, the
  pointer, and a mode integer.
- **EXAMPLE** (bad): `examples/bad/jni_array_release_missing.c` — sums the
  array and returns without Release.
- **COUNTEREXAMPLE** (good): `examples/good/jni_correct_references.c` —
  `(*env)->ReleaseIntArrayElements(env, arr, elems, 0)` before the return.
- **VERIFICATION**: static audit: every Get*ArrayElements has a matching
  Release*ArrayElements on all paths. UNVERIFIED as a run; the API pair is
  KNOWN.
- **SOURCE**: jni-spec (array operations); android-ndk.

## 5. Fingerprint the module before hooking with Frida

- **RULE**: Frida hooks must target real addresses. The correct first step
  is to fingerprint: `Process.findModuleByName` for base/size, then
  `Module.enumerateExports` for the export table. If the target symbol is
  absent (stripped or `RegisterNatives`-based), hook `RegisterNatives`
  inside `JNI_OnLoad` to observe the dynamic registration instead of
  guessing an address that will never fire.
- **WHY AI GETS IT WRONG**: agents write `Interceptor.attach(Module.getExportByName(...))`
  from memory of the symbol name and assume it exists; for stripped or
  packed code the hook silently never fires and the agent concludes "no
  calls happen".
- **CORRECT REASONING**: the hook is evidence only if it fires. Verify the
  export exists, or find the registration path, before attaching. The
  fingerprint log (module base, exports) is the artifact proving the hook
  target is real.
- **EXAMPLE** (bad): attaching to a guessed `Java_com_example_Main_check`
  export in a stripped `.so`.
- **COUNTEREXAMPLE** (good): `examples/good/frida_fingerprint.js` —
  enumerates base/size and the export table, printing each export address
  before any hook.
- **VERIFICATION**: `frida -U -l examples/good/frida_fingerprint.js
  com.example.app` prints the real module info; only then attach.
  UNVERIFIED as a run (no frida/device); the method is KNOWN.
- **SOURCE**: frida-docs (Process/Module API); android-ndk (RegisterNatives
  usage in NDK code).

## 6. "The hook ran" is not proof — verify the hook fires

- **RULE**: a Frida script that loads without error is not evidence the
  target was intercepted. Count hits (`hit_count++`), log arguments, and
  confirm the count is non-zero after the app runs. This is the JNI-world
  application of `meta-verification-harness-validity`: the harness (hook)
  must demonstrably fail when the target is absent.
- **WHY AI GETS IT WRONG**: the agent loads the script, sees no errors,
  and reports "hooked"; the abort/health check shows the target function
  was never called.
- **CORRECT REASONING**: instrument the instrumentation: a counter and a
  log line per invocation, then exercise the app path that calls the
  function and confirm hits > 0. Zero hits = wrong symbol/address.
- **EXAMPLE** (bad): a hook on a nonexistent export that loads silently
  and reports nothing.
- **COUNTEREXAMPLE** (good): fingerprint-first (rule 5) plus hit counting
  so the evidence includes "the hook actually fired".
- **VERIFICATION**: the script must print at least one hit for the target
  invocation path. UNVERIFIED as a run; the principle is KNOWN.
- **SOURCE**: frida-docs; meta-verification-harness-validity (the
  harness-validity principle).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| symbol name | Java_<mangled_pkg>_<mangled_class>_<method>(JNIEnv*, jobject/jclass, ...) |
| JNIEnv | pointer to function table; every call is (*env)->F(env, ...) |
| local refs | auto-release at frame exit; always NULL-check |
| global refs | NewGlobalRef must be DeleteGlobalRef'd (else ART overflow abort) |
| arrays | GetIntArrayElements must be paired with ReleaseIntArrayElements |
| Frida | fingerprint module + exports first; hook real addresses |
| hook validity | count hits; zero hits = wrong target |
