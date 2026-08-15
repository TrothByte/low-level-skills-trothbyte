#!/bin/sh
# intentionally incorrect
# The build's exit code is never captured, so a failed build is reported as
# success and the stale artifact is used anyway. "Command ran" is not
# "command succeeded".
ninja -C build
echo "build finished"            # overwrites $? with 0
cat build/app >/dev/null 2>&1    # runs even when ninja failed
