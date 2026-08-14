// C source that generated the verified Windows x64 prologues in this directory
// (GCC 16.1, MinGW, -O0/-O1/-O2 as noted in each .s file).
long long f64(long long a, long long b, long long c, long long d,
              long long e, long long f6, long long g, long long h)
{
    return a + b + c + d + e + f6 + g + h;
}

long long callee(long long a, long long b, long long c, long long d);
long long caller(void) { return callee(1, 2, 3, 4); }
