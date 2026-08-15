#!/bin/sh
# seccomp + capabilities probe: read the kernel's own accounting.
set -u
echo "== seccomp mode =="
grep -E "^(Seccomp|Seccomp_filters):" /proc/self/status
echo "== capability sets (hex masks) =="
grep -E "^(CapInh|CapPrm|CapEff|CapBnd|CapAmb):" /proc/self/status
echo "== human-readable caps (if capsh present) =="
capsh --print 2>/dev/null | head -10 || echo "capsh not installed"
