#!/bin/sh
# Check the real exit code of the build AND prove the artifact was produced and
# changed since the last recorded success. Exit 0 alone is never "done".
set -u

ninja -C build
rc=$?
[ "$rc" -ne 0 ] && { echo "build failed (rc=$rc)" >&2; exit "$rc"; }

test -f build/app || { echo "build exited 0 but produced no app" >&2; exit 1; }

old="$(cat build/.app.stamp 2>/dev/null || true)"
now="$(stat -c '%Y:%s' build/app)"
if [ "$old" = "$now" ]; then
    echo "app unchanged since last successful build - suspicious" >&2
    exit 1
fi
echo "$now" > build/.app.stamp
echo "build ok: app $now"
