# c — Skills

C is the lingua franca of low-level engineering.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `c-errno-and-syscall-returns` | Use when writing or reviewing C code that calls libc or system calls which report errors through errno and negative return values — read, write, accept, connect, open, close, strtol. Covers errno discipline, EINTR retry loops, partial read/write handling, EOF detection, and fd validation. | common | source-backed | `skills/c/c-errno-and-syscall-returns` |
| `c-integer-promotion-and-conversion` | Use when writing or reviewing C arithmetic where signed/unsigned mixing, integer promotion, narrowing, or size_t vs int conversions can cause wrong results or overflow — comparisons, array sizes, allocation sizes, and length calculations. Teaches the usual arithmetic conversions and the classic wrap surprises. | improved | source-backed | `skills/c/c-integer-promotion-and-conversion` |
| `c-signal-handler-safety` | Use when writing or reviewing C code that installs or runs signal handlers — SIGINT/SIGTERM shutdown, async flag set, EINTR handling, crash handlers. Covers async-signal-safe functions, volatile sig_atomic_t, sigaction vs signal, self-pipe trick, and signal behavior in multithreaded programs. | common | source-backed | `skills/c/c-signal-handler-safety` |
| `c-string-and-buffer-safety` | Use when writing or reviewing C code that copies strings or fills buffers — strncpy, snprintf, strcpy, strcat, memcpy/memmove, sizeof on arrays vs pointers, _FORTIFY_SOURCE, or anything that can overrun or fail to NUL-terminate a buffer. | common | source-backed | `skills/c/c-string-and-buffer-safety` |
| `c-undefined-behavior` | Use when writing, reviewing, or debugging C code where undefined behavior (UB) may be present — signed overflow, out-of-bounds access, uninitialized reads, invalid pointers, shift/aliasing violations, or when a program behaves differently across optimization levels or compilers. Teaches the J.2 UB taxonomy and how to detect each class. | improved | source-backed | `skills/c/c-undefined-behavior` |
| `checked-c-migration` | Use when adding Checked C bounds annotations to legacy C, or when a memory-safety retrofit needs checked pointers, _Array_ptr, bounds declarations, or incremental migration. Teaches annotation patterns LLMs can propose and how to verify them, distinct from rewriting in Rust. | unique | researched | `skills/c/checked-c-migration` |
| `endianness-and-byte-order` | Use when serializing, parsing, or writing wire/file formats — byte order, htonl/ntohl, network byte order, endianness detection, or portable struct deserialization. Teaches shift-based parsing and the pitfalls of struct memcpy and union type punning across architectures. | common | source-backed | `skills/c/endianness-and-byte-order` |
| `floating-point-ieee-semantics` | Use when writing or reviewing floating-point code — NaN/Inf handling, -ffast-math effects, x87 80-bit excess precision, rounding, FMA contraction, or when FP results differ across optimization levels or platforms. Teaches IEEE 754 semantics LLMs assume wrong. | common | source-backed | `skills/c/floating-point-ieee-semantics` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
  (`references/` hold deep knowledge; `examples/good` and `examples/bad` are compiled/run
  fixtures; `evals/README.md` defines eval cases.)
- Load only the skill you need (see `skills/_meta/meta-routing`; references load on demand.

## Related

[Back to repository root](../../README.md)
