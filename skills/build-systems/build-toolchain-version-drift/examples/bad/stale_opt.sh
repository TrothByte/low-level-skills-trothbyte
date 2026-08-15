// intentionally incorrect
// Claims a "-O3" build, but -O3 never reaches the compiler. The resulting
// binary is byte-identical in .text to the -O0 build; the agent then reports
// "optimization made no difference", which is a build-config bug, not a fact.
// The fix is to inspect the real compile command and the .text section, never
// to trust the echo.
set -e

gcc -O0 opt.c -o app.exe

echo "built with -O3: OK"
