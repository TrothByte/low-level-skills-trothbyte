// intentionally incorrect: JNI reference misuse.
// Local references (NewStringUTF, FindClass, GetObjectArrayElement) are
// released by JNI at the end of the native call automatically, but
// GLOBAL references (NewGlobalRef) MUST be deleted with DeleteGlobalRef.
// Leaking global refs eventually crashes ART with JNI Global Reference
// overflow. This function creates a global ref every call and never
// releases it.
#include <jni.h>

JNIEXPORT void JNICALL
Java_com_example_Main_acquireGlobals(JNIEnv *env, jobject thiz) {
    (void)thiz;
    jclass cls = (*env)->FindClass(env, "java/lang/String");
    if (cls == NULL) {
        return;
    }
    // WRONG: promoted to global and never deleted -> leak per call.
    jclass global = (jclass)(*env)->NewGlobalRef(env, cls);
    (void)global;
}

// WRONG: NewStringUTF result used without checking for NULL (OOM), and
// never released as a local ref (released at frame exit, but the NULL
// check is mandatory).
JNIEXPORT jstring JNICALL
Java_com_example_Main_makeString(JNIEnv *env, jobject thiz) {
    (void)thiz;
    return (*env)->NewStringUTF(env, "hello");
}
