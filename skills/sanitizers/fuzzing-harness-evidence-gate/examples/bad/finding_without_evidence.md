# intentionally incorrect
# A "finding" with none of the three evidence-gate artifacts: no saved input, no
# reproducer command, no coverage, no sanitizer set, no reachable path
# (reference rule 5, 9).

## Finding: heap-buffer-overflow in pktparse

Fuzzing the pktparse parser for two minutes found a heap-buffer-overflow.
A crash file was seen once during the run.

- No artifact was saved.
- No reproducer command was recorded.
- Coverage and runtime were not measured.
- The sanitizer set is not stated.

Conclusion offered: "pktparse has a heap buffer overflow."
