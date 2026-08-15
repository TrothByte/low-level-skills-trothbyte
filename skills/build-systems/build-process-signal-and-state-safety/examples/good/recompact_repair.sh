#!/bin/sh
# Repair ninja's internal logs after an interrupted build instead of wiping the
# build tree. Recompact rewrites the logs compactly; it does not restore entries
# already lost, so confirm state and let a targeted rebuild recover the rest.
set -u

ninja -t recompact
rc=$?
[ "$rc" -ne 0 ] && { echo "recompact failed (rc=$rc)" >&2; exit "$rc"; }

ninja -t deps main.o
ninja -n
