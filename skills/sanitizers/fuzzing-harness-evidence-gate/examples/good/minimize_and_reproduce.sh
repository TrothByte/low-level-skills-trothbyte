# Good discipline: artifact saved, reproduced on the pinned binary, minimized,
# then re-verified on the minimized input (rule 5, 6).
#!/bin/sh
set -eu
clang -O1 -g -fsanitize=fuzzer,address -fno-omit-frame-pointer \
      -o fuzz_pkt fuzz_pkt.c
./fuzz_pkt -artifact_prefix=./art/ -max_total_time=600 ./corpus
./fuzz_pkt -runs=1 ./art/crash-*
./fuzz_pkt -minimize_crash=1 -runs=1 ./art/crash-*
./fuzz_pkt -runs=1 ./art/minimized-from-*
