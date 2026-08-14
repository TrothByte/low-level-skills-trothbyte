// BAD: optimized build. Same code shape as examples/good but compiled with -O2.
// Compile: gcc -O2 -g -o optimized_away.exe optimized_away.c
// At -O2 locals are assigned to registers or eliminated entirely; a variable
// whose value has no home at some PC has no DW_AT_location, so the debugger
// reports "value optimized out".

static inline int triple(int x) {   // inlined into use_reg; `y` may not exist
    int y = x * 3;
    return y;
}

int folded_sum(int n) {
    int sum = 0;                    // loop folded to n*(n+1)/2 -> sum, i have no location
    for (int i = 1; i <= n; i++) {
        sum += i;
    }
    return sum;
}

int use_reg(int a, int b) {
    int prod = a * b;               // kept in a register only; location is register-relative
    return prod + triple(prod);
}

int main(int argc, char **argv) {
    volatile int seed = argc;       // prevents constant-folding of the calls below
    int s = folded_sum(seed);
    int u = use_reg(seed, seed + 1);
    return s + u;
}
