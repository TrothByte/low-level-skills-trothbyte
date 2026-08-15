// intentionally incorrect
// Declares but never defines missing_func. The link fails with a single
// "undefined reference"; nm/objdump -t on the object show the symbol in the
// undefined section. The fix is a definition (or a real library that provides
// it) - not a rewrite of the caller or a random -l flag.
int missing_func(int);

int main(void) { return missing_func(1); }
