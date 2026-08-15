# intentionally incorrect
# The crash corpus is never minimized and never reproduced on a pinned binary,
# so the finding ships a multi-megabyte input with no reachable path proven
# (reference rule 5, 6).
#!/bin/sh
set -u
afl-fuzz -i in -o out -- ./target @@
crash=$(ls out/crashes/id:* 2>/dev/null | head -1)
cp "$crash" ./report/crash.raw
echo "found crash: $crash"
echo "claim filed"   # gate fails: no -runs=1 repro, no afl-tmin, no stack proof
