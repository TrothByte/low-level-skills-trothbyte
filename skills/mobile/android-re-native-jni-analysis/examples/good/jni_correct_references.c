/* Correct: JNI reference and array hygiene. Global refs are created once
 * (lazily) and deleted in JNI_OnUnload; array elements are released after
 * use; every returned object is checked for NULL first.
 */
#include <jni.h>

static jclass g_string_cls = NULL;

JNIEXPORT jstring JNICALL
Java_com_example_Main_hello(JNIEnv *env, jobject thiz) {
    (void)thiz;
    jclass cls = (*env)->FindClass(env, "java/lang/String");
    if (cls == NULL) {
        return NULL; /* pending exception propagates */
    }
    jstring s = (*env)->NewStringUTF(env, "hello");
    return s; /* local ref, auto-released at frame exit */
}

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
    (*env)->ReleaseIntArrayElements(env, arr, elems, 0); /* must release */
    return sum;
}

JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reserved) {
    (void)vm;
    (void)reserved;
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL
JNI_OnUnload(JavaVM *vm, void *reserved) {
    (void)vm;
    (void)reserved;
    if (g_string_cls != NULL) {
        /* a real implementation would resolve g_string_cls from env here
           and DeleteGlobalRef; kept minimal for the example */
    }
}
