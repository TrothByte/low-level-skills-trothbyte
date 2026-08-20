# Endianness and Byte Order — Reference

Companion to `skills/c/endianness-and-byte-order/SKILL.md`. Deeper mechanics,
the exact standard rules, and the historical record. Depth lives here; the
SKILL.md stays operational.

## 1. What endianness is

Multi-byte integer types have no single memory layout. Endianness is the
choice of which byte is stored at the lowest address:

- **Little-endian** (x86, x86-64, ARM64-le, most microcontrollers): least
  significant byte first. `uint16_t 0x1234` in memory: `34 12`.
- **Big-endian** (s390/z, PowerPC BE, some ARM configs, the network): most
  significant byte first. `uint16_t 0x1234` in memory: `12 34`.
- Network byte order is big-endian by convention (RFC 1700 / STD 2).

The C standard makes endianness **implementation-defined**: the same object
may be laid out differently on another implementation, so a program whose
behavior depends on it is not strictly conforming.

- **SOURCE**: N1570 §6.2.6.1 (representation of types; "implementations
  may... store objects with high-order bytes last"). Memory layout itself is
  governed by §6.2.6 (object representation) plus the ABI.

## 2. The htonl family and its limits

| Function | Purpose | Portability |
|---|---|---|
| `htonl` / `htons` | host → network (big-endian) 32/16-bit | POSIX, Windows (winsock2) |
| `ntohl` / `ntohs` | network → host 32/16-bit | POSIX, Windows (winsock2) |
| `htobe64` / `be64toh` | 64-bit host ↔ big-endian | POSIX/Linux `<endian.h>` only |
| `htonll` / `ntohll` | 64-bit host ↔ network | **non-standard; absent from MSVC** |

Key facts:

1. On a big-endian host, `htonl`/`ntohl` are no-ops — but the code must not
   assume that; it should still route through the functions.
2. There is no portable 64-bit conversion in either POSIX or the C standard.
   Linux provides `htobe64`/`be64toh`; Windows provides nothing equivalent,
   so the name `htonll` is an invented helper and will not compile there.
   Portable substitute: the shift-based functions below.
3. `htons`/`htonl` convert the *value*, not memory. `uint16_t x = htons(0x1234)`
   yields 0x3412 on a little-endian host and 0x1234 on a big-endian host.

- SOURCE: POSIX `htonl(3)` man page (man7.org); Linux `byteorder(3)` /
  `endian.h` documentation.

## 3. Why native-struct serialization is not portable

```c
typedef struct { uint8_t tag; uint16_t kind; uint32_t value; } rec;
fwrite(&r, sizeof r, 1, f);          /* non-portable */
memcpy(buf, &r, sizeof r);           /* non-portable */
```

Two independent problems bake in:

1. **Padding**: `sizeof(rec)` includes implementation-chosen padding (on
   x86-64 this struct is 8 bytes: 1 byte `tag`, 1 pad, 2 `kind`, 4 `value`).
   A different ABI pads differently, so even the *size* differs.
2. **Endianness**: the multi-byte members are written in host order. The
   identical code on a big-endian host produces a different stream.

Therefore the wire/file format of such code is "whatever this host's ABI
happens to be". Correct approach: serialize each field explicitly.

- SOURCE: N1570 §6.7.2.1 (struct/union layout and padding); observed
  empirically in `examples/bad/struct_fwrite.c` (8-byte layout, bytes
  `ab 00 34 12 ef be ad de` for `{0xAB, 0x1234, 0xDEADBEEF}` on x86).

## 4. Shift-based portable serialization

```c
static void put_u32_be(uint8_t buf[4], uint32_t v) {
    buf[0] = (uint8_t)(v >> 24);
    buf[1] = (uint8_t)(v >> 16);
    buf[2] = (uint8_t)(v >> 8);
    buf[3] = (uint8_t)(v);
}

static uint32_t get_u32_be(const uint8_t buf[4]) {
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16)
         | ((uint32_t)buf[2] << 8) | ((uint32_t)buf[3]);
}
```

Rules that keep this correct:

- Every shift operand is unsigned (`uint32_t`); shifting a negative `int`
  into the sign bit is UB (N1570 §6.5.7).
- Each byte is masked by the cast to `uint8_t` on store; on load the byte is
  widened to `uint32_t` before shifting.
- The golden vector: `put_u32_be(buf, 0x12345678)` must give `12 34 56 78`
  on every implementation; `get_u32_be` must round-trip it.

- SOURCE: N1570 §6.5.7 (shift operators), §6.5p5 (signed overflow UB);
  verified in `examples/good/portable_serialize.c`.

## 5. Type punning: what is and is not defined

The only standard-conforming ways to inspect the bytes of an integer:

```c
unsigned char bytes[4];
memcpy(bytes, &x, sizeof x);   /* OK — object representation access */
```

Defined: `memcpy`, and reading via `unsigned char*` (the object
representation rules of N1570 §6.2.6.1 permit accessing via
`unsigned char`/`char` lvalues).

Undefined or non-portable:

- `*(uint32_t*)buf` — strict aliasing violation (N1570 §6.5p7) plus
  alignment hazard plus host byte order.
- Union punning (`union { uint32_t u; uint8_t b[4]; }`, write `u`, read `b`)
  — reading the inactive member is UB in ISO C. GCC and Clang implement it
  as an accepted extension (so `-Wstrict-aliasing` stays silent even at
  `-O3 -Wstrict-aliasing=3`), which misleads review. The value is still
  endianness-dependent.
- Pointer casts of any integer width: same three problems.

The `memcpy` version compiles to the identical machine code on x86 — there
is no performance excuse for the UB form.

- SOURCE: N1570 §6.5p7 (effective type), §6.2.6.1 (object representation);
  empirical: gcc 16.1 emits no `-Wstrict-aliasing` warning for the union
  pattern at `-O3 -Wstrict-aliasing=3`.

## 6. Alignment and unaligned access

`*(uint16_t*)p` where `p` is odd is UB: an object of type `uint16_t` must be
aligned to its alignment (N1570 §6.3.2.3, §6.5p6). Consequences differ:

- x86-64: tolerated in practice (unaligned loads work).
- ARM (pre-v7) and MIPS: data-abort / alignment trap, kernel OOPS.
- Optimizers may assume alignment and vectorize away the access.

Rule: assemble multi-byte fields from a byte stream with shifts, or route
through `memcpy`. Never cast a byte pointer to a multi-byte integer pointer
to "read the value".

- SOURCE: N1570 §6.5p6 (alignment); empirical: `examples/bad/unaligned_cast.c`
  runs on x86 (prints 0x5634) but is UB; UBSan `-fsanitize=undefined`
  reports "load of misaligned address" (needs a toolchain with `libubsan`;
  MinGW 16.1 on this host cannot link it).

## 7. Detecting endianness

Preferred: compile time.

```c
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  /* little-endian path */
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  /* big-endian path */
#endif
```

`__BYTE_ORDER__`/`__ORDER_LITTLE_ENDIAN__` are GCC/Clang builtins, so this
works on the common toolchains but not MSVC; guard with `#ifdef`.

Runtime fallback (portable, memcpy-based — no UB, no union):

```c
uint32_t magic = 0x01020304u;
unsigned char bytes[4];
memcpy(bytes, &magic, sizeof magic);
return bytes[0] == 0x01 ? BIG : LITTLE;   /* bytes[0] == 0x04 => little */
```

Anti-patterns:

- `if (*(char*)&i == 1)` — works but relies on pointer punning of an object
  whose representation you then read via `char*` (allowed) — the *problem*
  is that agents build convoluted versions that the optimizer or the
  aliasing rules break.
- Bitfield-based detection: bitfield allocation order is
  implementation-defined and not even tied to endianness; the result is
  meaningless across ABIs.

- SOURCE: gcc documentation for `__BYTE_ORDER__`; N1570 §6.7.2.1p11
  (bitfield allocation implementation-defined); verified in
  `examples/good/endian_probe.c` (this host: little-endian).

## 8. Bitfields are not a byte-order mechanism

Bitfields pack *bits*, and which bits get which field is decided by the
implementation (allocation order, padding, unit width). Two ABIs can put
`version:4` in the high nibble or the low nibble of the same byte. Observed
on this host: `version=0x3, type=0xA` packs to byte `0xA3` (LSB-first
allocation); a MSB-first ABI produces `0x3A`. Wire formats must assemble
bits with shifts.

- SOURCE: N1570 §6.7.2.1p11; empirical: `examples/bad/bitfield_hdr.c`.

## 9. Reading the format spec

Nearly all binary formats state their byte order explicitly (big-endian is
the wire convention: TCP/IP, DNS, RTP, most image formats). The agent's job:
implement the spec's order with shifts, regardless of the host. If the spec
is silent or the format is host-native (some ELF/DWARF sections, PGM-style
headers), the format's own header must carry the byte order explicitly — do
not assume.

## 10. Historical endianness bugs (eval material)

- NTP-era network implementations that treated host byte order as network
  order and worked only on big-endian research machines.
- Kerberos v4 byte-order problems in DES key schedules and packet headers
  (endianness assumptions baked into the protocol), fixed in v5 with
  explicit octet order.
- TCP/IP early implementations where header fields were accessed as native
  structs (`struct ip *ip = (struct ip*)packet`) — field order and padding
  differed across hosts; modern stacks use explicit accessors
  (e.g. `ntohs(ip->ip_len)`-style).
- Classic file-format parser bugs where a JPEG/GIF/BMP field read via a
  native cast produced different dimensions on big-endian hosts.

## 11. Verification workflow

1. Golden vectors on the host (x86): `examples/good/portable_serialize.c`
   must PASS.
2. Checker on all example sources: `endian_check.py` clean on `good/`,
   findings on `bad/`.
3. Big-endian leg: compile and run the good + bad examples under
   `qemu-s390x` (or `qemu-ppc64`): golden vectors must still PASS, and the
   bad examples must visibly flip byte order.
4. Sanitizer leg (Linux/Clang, where `libubsan` links): run
   `examples/bad/unaligned_cast.c` and `union_punning.c` under
   `-fsanitize=undefined` and confirm reports or, for the union, document
   that GCC/Clang silence it as an extension.

- SOURCE: N1570; RFC 1700/STD 2 (network byte order convention);
  historical record per section 10.

## Sources and primary material

- POSIX `htonl`/`htons`/`ntohl`/`ntohs` — man7.org (man3)
- ISO C11 N1570 — §6.2.6, §6.3.2.3, §6.5, §6.5.7, §6.7.2.1
- RFC 1700 / STD 2 — Network Byte Order convention
- Wikipedia: Endianness (overview and the mixed-endian/PDP-endian edge cases)
- Linux `byteorder.h` / `endian.h` documentation (64-bit helpers)
