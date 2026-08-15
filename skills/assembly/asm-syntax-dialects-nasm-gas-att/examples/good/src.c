/* GOOD: this C file is the input for the syntax-dialect demonstration.
 *   gcc -O2 -S src.c          -> AT&T output (default)
 *   gcc -O2 -masm=intel -S src.c -> Intel-syntax output (GAS/Intel dialect)
 * The two outputs encode the same instructions with reversed operand order.
 * Recorded output of both runs lives in evals/README.md.
 */
int f(int x, int *p) {
    return x * 38 + *p;
}
