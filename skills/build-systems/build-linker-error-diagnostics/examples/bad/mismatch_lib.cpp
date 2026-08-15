// intentionally incorrect
// The only definition of "add" in the project. Because the parameter type is
// double, the mangled name is _Z3addd - which does not satisfy a caller that
// needs _Z3addi. Linking mismatch_app.cpp against this file fails.
double add(double x) { return x; }
