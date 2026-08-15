// intentionally incorrect: JNI function signature mismatch.
// Java native declaration:
//   native long addInts(int a, int b);
// The C function MUST be:  jlong Java_com_example_Main_addInts(JNIEnv*,
// jobject, jint, jint). This file declares a (void) signature — the native
// library loads but the symbol name/signature do not match the Java side,
// so the call fails with UnsatisfiedLinkError at runtime.
#include <jni.h>
#include <stdint.h>

// WRONG: wrong name and no JNIEnv/args.
int64_t Java_com_example_Main_addInts(void) {
    return 42;
}

// WRONG: mismatched name (com_example vs com_example2) and missing
// jobject parameter. JNI symbol lookup is exact.
JNIEXPORT jint JNICALL
Java_com_example2_Main_addInts(JNIEnv *env) {
    (void)env;
    return 0;
}
