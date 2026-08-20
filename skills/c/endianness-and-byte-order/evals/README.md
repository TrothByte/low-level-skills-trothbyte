# Evaluation — endianness-and-byte-order

Skill: `skills/c/endianness-and-byte-order`. Stability: `source-backed`
(the host toolchain directly demonstrates byte-order semantics), with
verification legs defined for a big-endian target and UBSan.

## Verified facts (host, recorded 2026-08-20)

Host: Windows x86-64, gcc 16.1.0 (MSYS2/ucrt64), Python 3.11.9. All output
captured from real runs of the example programs.

- `examples/good/portable_serialize.c` (`-Wall -Wextra -Werror -O2`):
  golden vector u32 0x12345678 -> `12 34 56 78` PASS; golden vector u16
  0x1234 -> `12 34` PASS; roundtrip PASS; memcpy punning of 0x11223344 ->
  `44 33 22 11` (host little-endian). Program exits 0.
- `examples/good/endian_probe.c`: `host is little-endian`.
- `examples/bad/struct_fwrite.c`: `sizeof = 8`; host bytes
  `ab 00 34 12 ef be ad de` for `{tag=0xAB, kind=0x1234, value=0xDEADBEEF}`
  — one byte of padding at offset 1, and every multi-byte field stored
  LSB-first.
- `examples/bad/union_punning.c`: prints `04 03 02 01` (host byte order).
  gcc emits no `-Wstrict-aliasing` diagnostic even at
  `-O3 -Wstrict-aliasing=3 -fstrict-aliasing` — GCC/Clang implement union
  punning as an accepted extension; ISO C 6.5p7 still makes it UB, and the
  byte order flips on big-endian hosts.
- `examples/bad/unaligned_cast.c`: on x86 the unaligned `uint16_t` load
  works and prints `0x5634`; `-fsanitize=undefined` could not link on this
  host (`cannot find -lubsan`, MinGW ucrt64 lacks the runtime) — the UBSan
  leg requires a Linux/Clang toolchain.
- `examples/bad/htonll_usage.c`: compile fails with
  `#error "htonll/ntohll do not exist on Windows..."` followed by
  `implicit declaration of function 'htonll'` — proves the invented-helper
  failure on MSVC-family hosts.
- `examples/bad/bitfield_hdr.c`: `version=0x3 type=0xa` packs to raw byte
  `0xa3` on this ABI (LSB-first allocation); an MSB-first ABI gives `0x3a`.
- `examples/tools/endian_check.py` on good examples: `endian_check: clean`,
  exit 0. On bad examples: 7 findings (struct fwrite, struct memcpy, union
  punning, two pointer casts, htonll, two bitfields), exit 1.

## Synthetic evals

- easy/positive: shift-based serializer with golden vector — must not be
  flagged by `endian_check.py` and must PASS on both endiannesses.
- easy/negative: struct `fwrite` to a file — checker must flag it, and the
  agent must replace it with field-by-field shifts.
- medium/negative: union type punning for parsing — must be detected as UB
  even though gcc stays silent.
- medium/negative: `*(uint32_t*)buf` read on a byte stream — must be
  flagged; agent must convert to shift assembly or `memcpy`.
- hard/negative: 64-bit field conversion on a portable codebase using
  `htonll` — must be replaced with a shift-based `put_u64_be`, not with a
  guessed helper name.
- hard/negative: unaligned field read that "works on x86" — must be
  rewritten via `memcpy` before it reaches a strict-alignment target.

## False-positive evals (correct code must not be flagged)

- `memcpy(bytes, &x, sizeof x)` scalar type punning (portable_serialize.c)
  — legitimate; `endian_check.py` must stay clean on it.
- `htonl`/`ntohl`/`htons`/`ntohs` calls — valid, must not be flagged.
- `#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__` branches — valid.
- Shift-based `put_u32_be`/`get_u32_be` — valid, zero findings.
- `memcpy` into a struct (reading back a native record on the same host,
  no wire boundary) — not a serialization anti-pattern.

## Historical evals

- NTP/Kerberos-era byte-order bugs: early implementations read protocol
  headers as native structs or assumed the research-machine byte order;
  Kerberos v4's endianness-ambiguous DES key handling was reworked in v5 to
  explicit octet order. Agent must name the class (host order assumed ==
  wire order) and the fix (explicit byte order + accessors).
- TCP/IP stack history: `struct ip *ip = (struct ip*)packet` was common in
  early implementations; padding and endianness made it non-portable, and
  modern stacks use `ntohs`-style field accessors. Agent must identify the
  native-struct-header pattern as the root cause.
- File-format parser bugs: dimensions/sizes read via native casts from
  JPEG/GIF/BMP headers produced wrong values on big-endian hosts. Agent
  must require golden vectors in both byte orders.

## Adversarial evals

- Code that passes the full test suite on x86 because every fixture is
  little-endian: the struct-fwrite serializer with a golden vector that was
  recorded from the host (so it "matches" on x86 only). Agent must notice
  the golden vector is host-derived, not spec-derived.
- A "portable" header using bitfields whose unit-width happens to match on
  x86 — agent must reject it without a cross-ABI check.
- A union punning read that the optimizer keeps because the byte order is
  tested only on little-endian — agent must identify the endianness
  dependence, not just the aliasing hazard.
- A comment claiming "htonll is available everywhere" next to code using it
  — agent must not trust the claim; the compile-time #error is the truth.

## Verification commands (target — big-endian QEMU, -fsanitize=undefined)

```
gcc -Wall -Wextra -Werror -O2 examples/good/portable_serialize.c && ./a.out
gcc -Wall -Wextra -Werror -O2 examples/good/endian_probe.c && ./a.out
python examples/tools/endian_check.py examples/good/*.c    # clean, exit 0
python examples/tools/endian_check.py examples/bad/*.c     # 7 findings, exit 1

# big-endian leg (requires qemu-user + cross gcc on Linux):
s390x-linux-gnu-gcc -Wall -Wextra -Werror -O2 examples/good/portable_serialize.c
qemu-s390x ./a.out        # golden vectors still PASS; endian_probe -> big-endian

# sanitizer leg (Linux/Clang toolchain with libubsan; MinGW here cannot link it):
clang -O2 -g -fsanitize=undefined -fno-sanitize-recover=undefined \
      examples/bad/unaligned_cast.c && ./a.out
#   -> UBSan: "runtime error: load of misaligned address" (x86-64, -fsanitize=alignment)
```

Expected on the big-endian leg: `portable_serialize` PASS (byte order is
host-independent), `struct_fwrite` prints big-endian field bytes and
different padding, `union_punning` prints `01 02 03 04`, `endian_probe`
prints big-endian.

## Scoring

- precision: every flagged finding must map to a real endianness/aliasing
  defect (no findings on `examples/good/`).
- recall: all seven "what the agent often gets wrong" patterns must be
  caught by the checker or by review (union punning is caught by
  `endian_check.py` rule 5, not by gcc's silent `-Wstrict-aliasing`).
- FP-rate: zero on the false-positive eval list, including scalar `memcpy`
  punning.
- cross-architecture: golden vectors must pass on a big-endian emulated
  target before the skill is marked `verified`.
