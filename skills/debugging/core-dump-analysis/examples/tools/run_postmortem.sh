#!/usr/bin/env bash
# run_postmortem.sh — reproduce a crash under gdb, extract bt / registers /
# memory maps, and summarize with core_analyze.py.
#
# Usage:  bash run_postmortem.sh <binary> [output.txt]
#
# On Linux (gdb core files + `info proc mappings`) the script:
#   1. runs the binary under gdb and calls `generate-core-file` (core(5) setup
#      is assumed: `ulimit -c unlimited` for kernel-written cores, or use this
#      gdb path when the target was built with debug info);
#   2. loads the core back with `gdb <binary> core` for the extraction pass.
#
# On Windows MinGW gdb the same steps run, but two documented limitations
# apply (measured 2026-08-20, gdb 17.2): `generate-core-file` fails with
# "Can't create a corefile" and `info proc mappings` prints "Not supported on
# this target". The script falls back to extracting the evidence from the LIVE
# crash stop and uses `info files` section listings as the memory map. That
# fallback is what makes the workflow host-independent.
#
# Exit code is always 0 if gdb ran; inspect the summary, not the script exit.

set -u

BIN="${1:?usage: run_postmortem.sh <binary> [output.txt]}"
OUT="${2:-postmortem.txt}"
HERE="$(cd "$(dirname "$0")" && pwd)"
PY="$HERE/core_analyze.py"

LIVE_TXT="$(mktemp)"
CORE="$(mktemp -u postmortem.core.XXXXXX)"

# --- pass 1: live crash extraction (always) -------------------------------
gdb -batch \
  -ex "set pagination off" \
  -ex run \
  -ex "thread apply all bt" \
  -ex "bt" \
  -ex "info registers rip rsp rbp" \
  -ex "info proc mappings" \
  -ex "info files" \
  -ex 'x/24wx $rsp-0x40' \
  --args "$BIN" \
  > "$LIVE_TXT" 2>&1

# --- pass 2: core generation (best effort) --------------------------------
if gdb -batch -ex "set pagination off" -ex run -ex "generate-core-file $CORE" \
     --args "$BIN" >/dev/null 2>&1 && [ -f "$CORE" ]; then
  {
    echo "=== gdb: core file reload ($CORE) ==="
    gdb -batch -ex "set pagination off" -ex "thread apply all bt" -ex "bt" \
        -ex "info registers rip rsp rbp" -ex "info files" \
        "$BIN" "$CORE" 2>&1
  } >> "$LIVE_TXT"
  rm -f "$CORE"
else
  {
    echo "=== gdb: generate-core-file not supported on this target ==="
    echo "    live crash stop used as post-mortem evidence"
  } >> "$LIVE_TXT"
fi

cat "$LIVE_TXT" > "$OUT"
python "$PY" "$OUT"
rm -f "$LIVE_TXT"
