// intentionally incorrect
// Calls add(int) but the only definition available is add(double). C++ mangles
// names, so the linker looks for _Z3addi and reports "undefined reference to
// 'add(int)'" even though a function named "add" exists. The diagnostic is the
// symbol table, not the source: nm -C shows add(double).
int add(int);

int main() { return add(1); }
