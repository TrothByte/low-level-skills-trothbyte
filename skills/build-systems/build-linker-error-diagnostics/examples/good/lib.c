// The matching definition: same name, same signature, in the link set.
// Linking main.c + lib.c succeeds; the symbol is now DEFINED (nm shows T).
int missing_func(int x) { return x - 1; }
