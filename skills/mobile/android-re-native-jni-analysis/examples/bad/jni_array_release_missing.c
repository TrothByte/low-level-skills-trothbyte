// intentionally incorrect: calling GetArrayElements without Release
// and ignoring the JNI function table accessor mismatch. On Android the
// JNIEnv is a pointer to a function table; getting the table wrong (or
// using the wrong accessor variant for the environment) produces a crash.
// Here GetIntArrayElements pins the array but ReleaseIntArrayElements is
// never called — the pin leaks and, for a large array, can exhaust the
// pinned memory.
#include <jni.h>

JNIEXPORT jint JNICALL
Java_com_example_Main_sumArray(JNIEnv *env, jobject thiz, jintArray arr) {
    (void)thiz;
    if (arr == NULL) {
        return -1;
    }
    jint *elems = (*env)->GetIntArrayElements(env, arr, NULL);
    if (elems == NULL) {
        return -1;
    }
    jint sum = 0;
    jsize len = (*env)->GetArrayLength(env, arr);
    for (jsize i = 0; i < len; i++) {
        sum += elems[i];
    }
    // WRONG: no ReleaseIntArrayElements(env, arr, elems, 0).
    return sum;
}
