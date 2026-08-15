/* Correct: JNI function with exact symbol name, correct signature, and
 * reference hygiene. Java declaration:
 *   native long addInts(int a, int b);
 * Native symbol: Java_com_example_Main_addInts(JNIEnv*, jobject, jint, jint)
 */
#include <jni.h>

JNIEXPORT jlong JNICALL
Java_com_example_Main_addInts(JNIEnv *env, jobject thiz, jint a, jint b) {
    (void)env;
    (void)thiz;
    return (jlong)a + (jlong)b;
}
