# Finding report — pkt_parse heap-buffer-overflow (evidence-complete, rule 9)

## 1. Reproducible sanitizer report

- Binary: `fuzz_pkt` built from `fuzz_pkt.c` with
  `clang -O1 -g -fsanitize=fuzzer,address -fno-omit-frame-pointer`.
- Reproducer: `./fuzz_pkt -runs=1 art/crash-a1b2c3`
- Report: `ERROR: AddressSanitizer: heap-buffer-overflow` at
  `pkt_parse.c:42:18` in `parse_record` (16-byte region allocated in
  `pkt_parse.c:87:15`). Same header on every rerun.

## 2. Minimized crashing input

- `art/crash-a1b2c3` reduced by `-minimize_crash=1` from 140 KB to 27 bytes
  (`art/minimized-from-a1b2c3`).
- The report reproduces unchanged: `./fuzz_pkt -runs=1 art/minimized-from-a1b2c3`.

## 3. Demonstrated reachable path

- `-print_pcs=1` shows frames `LLVMFuzzerTestOneInput -> pkt_parse ->
  parse_record`; the fault is index 16 of a 16-byte payload (loop bound
  `i <= hdr->count`).

## 4. Coverage / runtime / limitations

- 120 s, 3 workers, 48k exec/s, corpus 220 inputs, line coverage 41% over the
  parse paths.
- Input carries a CRC-32 header field, so most mutations after byte 4 are
  rejected before parsing.
- ASan only; UB and uninitialized reads were NOT checked in this run.
