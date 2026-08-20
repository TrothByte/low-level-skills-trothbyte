---
name: endianness-and-byte-order
description: Use when serializing, parsing, or writing wire/file formats — byte order, htonl/ntohl, network byte order, endianness detection, or portable struct deserialization. Teaches shift-based parsing and the pitfalls of struct memcpy and union type punning across architectures.
---

# Endianness and Byte Order

## When to use

- Serializing or parsing any wire or file format where multi-byte integers
  cross a machine boundary: TCP/IP, UDP, CAN-over-ethernet, binary files,
  firmware images, RPC payloads, protocol headers.
- Writing or reviewing code that uses `htonl`/`htons`/`ntohl`/`ntohs`, or that
  needs 64-bit network-order conversion.
- Detecting the host byte order at compile time or at runtime.
- Converting integers to and from raw byte buffers (packing and unpacking
  fields of a streamed record).
- Reviewing code that `memcpy`/`fwrite`s native structs or unions "as-is" onto
  a stream, or casts a byte pointer to `uint16_t*`/`uint32_t*`.

## When not to use

- Text formats (JSON, CSV, ASCII protocols): no byte-order question exists.
- Formats where every field is exactly one byte: byte order only affects
  multi-byte values.
- In-process code with no serialization boundary on a single host: the host
  ABI is fine there, and endianness is a non-issue.
- C++ where `std::endian`/`std::bit_cast` is the appropriate tool; this skill
  is C-focused (see `ffi-boundary-cross-language` for mixed-language nuance).

## What the agent often gets wrong

- `memcpy`/`fwrite` of a native struct as serialization. "It works on my
  machine" because host padding and little-endian byte order are baked into
  the output; the file or wire format becomes host-specific.
- Union type punning (`union { uint32_t u; uint8_t b[4]; }`) to inspect bytes
  or parse. Reading the inactive member is UB in ISO C, and the byte order is
  endianness-dependent (and GCC/Clang accept it as an extension, which hides
  the problem).
- Assuming little-endian everywhere: `host == wire` only on x86/ARM64-le.
  Big-endian hosts (s390, PowerPC, network stacks) disagree.
- Inventing `htonll`/`ntohll`/`be64toh` on Windows. `htobe64`/`be64toh` are
  POSIX/Linux-only; MSVC has no direct equivalent, so the name does not exist.
- Using bitfields to parse or describe protocol records. Bitfield layout is
  implementation-defined (bit order, allocation order, padding) — never
  portable across ABIs.
- Forgetting alignment. `*(uint32_t*)p` on an unaligned address is UB and
  traps on strict-alignment targets (ARM pre-v7, MIPS); x86 masks it.
- Writing convoluted runtime endianness tests. A compile-time macro branch
  with a small `memcpy`-based fallback is simpler and cannot be miscompiled.
- Off-by-one in shift-based assembly: shifting the wrong byte, or shifting a
  signed `int` into the sign bit (signed shift UB).

## How to reason correctly

1. Read the format specification's declared byte order (nearly always
   big-endian on the wire) and implement it explicitly with shifts. Never let
   the host's layout leak into the stream.
2. Use `memcpy` — not unions, not pointer casts — when converting between an
   integer and a byte buffer. `memcpy` has no alignment or aliasing
   constraints and is the standard-conforming way to type-pun.
3. Where code must branch on endianness, use the compile-time macro
   `#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__` (GCC/Clang), and keep a
   portable `memcpy`-based fallback that is correct on any host.
4. Check alignment before any cast-based access. Prefer `memcpy` into an
   aligned local for unaligned field reads; it compiles to the same load on
   x86 and to correct code elsewhere.
5. When designing a new format, pick an explicit byte order (big-endian is the
   wire convention), document it, and funnel all access through one
   serialize/deserialize pair so the order is fixed in exactly one place.
6. Verify on a big-endian target (QEMU s390x/ppc64) and against byte-level
   golden vectors. If it only passes on x86, it is unverified.

## What to verify

- Serialization is shift-based or through explicit helpers; no struct
  `fwrite`/`memcpy` of native layout reaches the wire or file.
- No union or pointer punning for byte access anywhere in the path; `memcpy`
  is used instead.
- The produced bytes match the format spec (golden test vectors).
- Any endianness branch uses the preprocessor macro, and the portable
  fallback is exercised.
- No unaligned field accesses; where unavoidable, they go through `memcpy`.

## How to verify

```
gcc -Wall -Wextra -Werror -O2 examples/good/portable_serialize.c && ./a.out
gcc -Wall -Wextra -Werror -O2 examples/good/endian_probe.c && ./a.out
python examples/tools/endian_check.py examples/good/*.c    # expect: clean
python examples/tools/endian_check.py examples/bad/*.c     # expect: findings
```

Golden vector: `put_u32_be(0x12345678)` must yield bytes `12 34 56 78` on
every host. The bad examples compile and run on x86 but demonstrate why they
are non-portable; run them on a big-endian emulator to see the byte order
flip. Sanitizer note: this MinGW host lacks `libubsan`, so
`-fsanitize=undefined` fails at link time — run that leg on a Linux/Clang
toolchain or in QEMU.

## Where the knowledge comes from

- POSIX htonl/htons/ntohl/ntohs (https://man7.org/linux/man-pages/man3/htonl.3.html)
- C standard 6.2.6.1 (representation of types) — endianness implementation-defined
- Wikipedia: Endianness (https://en.wikipedia.org/wiki/Endianness)
- Network Byte Order — RFC 1700/STD 2 (https://www.rfc-editor.org/rfc/rfc1700.html)
- Linux byteorder.h docs

## Related skills

- `c-integer-promotion-and-conversion` — unsigned shifts and value conversion when assembling bytes
- `abi-layout-reasoning` — struct padding and layout that make native serialization non-portable
- `ffi-boundary-cross-language` — byte order when integers cross language boundaries
- `c-undefined-behavior` — aliasing, alignment, and union-punning UB classes
- `auto-re-protocols-beyond-can` — CAN protocol fields parsed from byte streams
- `reverse-engineering-can-signal-extraction` — reconstructing multi-byte signals from raw frames

## Evaluation

Verified facts recorded on the host (gcc 16.1 MinGW): golden vectors PASS,
struct fwrite output shows little-endian bytes plus padding, host probe
reports little-endian. Synthetic evals cover all seven failure modes above;
false-positive evals require the checker to stay clean on the good examples
(including legitimate scalar `memcpy` punning). Historical evals cover classic
endianness bugs in file-format parsers and early TCP/IP implementations;
adversarial evals hide the bugs behind x86-tolerance. Details, real outputs,
and scoring: `evals/README.md`.
