// Member object of libmulti.a. Symbol: bar (defined). NOT referenced by
// arch_main.c - it is only pulled into the executable under --whole-archive.
int bar(void) { return 9; }
