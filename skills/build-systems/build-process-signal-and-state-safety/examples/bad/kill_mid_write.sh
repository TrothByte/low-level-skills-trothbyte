#!/bin/sh
# intentionally incorrect
# Terminates the build mid-write and then treats the partial ninja state as
# valid. SIGTERM at the wrong moment truncates .ninja_deps/.ninja_log, so the
# next run recompiles everything and the "clean interruption" claim is false.
ninja -C build &
pid=$!
sleep 2
kill -TERM "$pid"        # ninja dies mid-fwrite of .ninja_deps / .ninja_log
wait "$pid" 2>/dev/null
echo "build interrupted cleanly"     # it was not clean at all
ninja -C build           # full recompile: "why is everything dirty?"
