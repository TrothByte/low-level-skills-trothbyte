// intentionally incorrect
// Twelve undefined references share one prefix (__ksym_N). The cascade is NOT
// twelve bugs: every name traces to one missing definition source (a generated
// table, an excluded object). Find the first error and the shared pattern, not
// the count. This is the mini analog of a 40k-error kernel link: all symbols
// come from one missing generated section (__jump_table/__ksymtab).
int __ksym_1(void); int __ksym_2(void); int __ksym_3(void);
int __ksym_4(void); int __ksym_5(void); int __ksym_6(void);
int __ksym_7(void); int __ksym_8(void); int __ksym_9(void);
int __ksym_10(void); int __ksym_11(void); int __ksym_12(void);

int main(void)
{
    __ksym_1(); __ksym_2(); __ksym_3(); __ksym_4();
    __ksym_5(); __ksym_6(); __ksym_7(); __ksym_8();
    __ksym_9(); __ksym_10(); __ksym_11(); __ksym_12();
    return 0;
}
