// Only foo is referenced by main. Linking against libmulti.a (foo.o + bar.o)
// pulls in ONLY foo.o unless --whole-archive is used. Verify with nm on the
// executable: without --whole-archive only "foo" appears; with it, both "foo"
// and "bar" appear.
int foo(void);

int main(void) { return foo() == 7 ? 0 : 1; }
