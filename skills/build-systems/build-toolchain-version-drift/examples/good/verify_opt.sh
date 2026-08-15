# Diagnose -O drift honestly. Build every level, compare the .text section
# (never whole executables: PE headers add timestamps and path bytes), and dump
# the active macros. If -O0 and -O3 .text are identical, the optimization flag
# never reached the compiler - a build-config bug, not a compiler fact.
set -e

gcc -O0 opt.c -o o0.exe
gcc -O2 opt.c -o o2.exe
gcc -O3 opt.c -o o3.exe

objcopy -O binary --only-section=.text o0.exe o0.text
objcopy -O binary --only-section=.text o2.exe o2.text
objcopy -O binary --only-section=.text o3.exe o3.text

if cmp -s o0.text o3.text; then
    echo "SUSPICIOUS: -O0 and -O3 .text are identical - flag never applied"
else
    echo "OK: -O0 and -O3 .text differ"
fi

gcc -x c++ -std=c++20 -dM -E - < /dev/null | grep __cplusplus
