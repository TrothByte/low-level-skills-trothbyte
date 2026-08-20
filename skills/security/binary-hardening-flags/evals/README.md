# Evaluation — binary-hardening-flags

Skill: `skills/security/binary-hardening-flags`.
Stability target: `evaluated`. Toolchain: gcc 16.1.0 (MSYS2 ucrt64, Windows),
binutils 2.46 (`objdump`, `readelf`).

## Verified facts (host, recorded 2026-08-20)

Every command below was actually executed on this host. Working directory is
`examples/` of this skill. Source files: `good/hardened.c` and `bad/plain.c`
(identical C, same stack-buffer function), `bad/canary_bypass.c`.

Toolchain versions:

```
gcc.exe (Rev5, Built by MSYS2 project) 16.1.0
GNU objdump (GNU Binutils) 2.46
GNU readelf (GNU Binutils) 2.46
```

Compile + run (hardened vs plain, identical source):

```
gcc -O2 -fstack-protector-strong -fcf-protection=full -D_FORTIFY_SOURCE=2 -o good/hardened.exe good/hardened.c   (exit 0)
gcc -o bad/plain.exe bad/plain.c                                                                                 (exit 0)

./good/hardened.exe "attack-payload"  ->  processed: attack-payload   (exit 0)
./bad/plain.exe "attack-payload"      ->  processed: attack-payload   (exit 0)
```

Canary detection demo — the SAME source compiled with and without the flag:

```
gcc -O2 -fstack-protector-strong -o bad/canary_on.exe bad/canary_bypass.c   (exit 0)
gcc -O2 -o bad/canary_off.exe bad/canary_bypass.c                           (exit 0)

./bad/canary_on.exe
  -> "*** stack smashing detected ***: terminated"   (exit -1073740791 = 0xC0000409 STATUS_STACK_BUFFER_OVERRUN)
./bad/canary_off.exe
  -> "AAAAAAAAAAAAAAA" / "RETURNED NORMALLY"         (exit 0, silent corruption)
```

The payload is 15 'A' + NUL (16 bytes) into an 8-byte stack buffer. Verified
by disassembly: buffer at `0x20(%rsp)`, canary at `0x28(%rsp)`, saved return
address at `0x38(%rsp)` — the overflow clobbers the canary but not the return
address, so the uncanaried build returns 0 while the canaried build aborts.

Binary evidence (objdump on the produced PE files):

```
objdump -t good/hardened.exe | grep stack_chk
  [117](sec  3)(fl 0x00)(ty    0)(scl   3) (nx 1) 0x0000000000000520 .rdata$.refptr.__stack_chk_guard
  [780](sec  1)(fl 0x00)(ty   20)(scl   2) (nx 1) 0x0000000000001770 __stack_chk_fail
objdump -d good/hardened.exe | grep chk
  1400014b1: e8 0a 13 00 00  call 1400027c0 <__strcpy_chk>     (FORTIFY redirect)
  1400014dd: e8 8e 12 00 00  call 140002770 <__stack_chk_fail>  (canary check)
objdump -d good/hardened.exe | grep endbr64
  140002b90: f3 0f 1e fa      endbr64                            (CET IBT marker at main)
objdump -t bad/plain.exe | grep stack_chk      -> (no output: no canary symbol)
objdump -d bad/plain.exe | grep -c endbr64     -> 0
objdump -d bad/plain.exe | grep strcpy         -> call 140002930 <strcpy>   (plain call, no _chk)
```

CET note finding (honest, target-dependent): `-fcf-protection=full` on this
MinGW PE target produced ONE `endbr64` (at `main`) but NO `.note.gnu.property`
section in the PE output (section list: .text/.data/.rdata/.pdata/.xdata/.bss/
.idata/.tls/.rsrc/.reloc plus DWARF). `readelf -n good/hardened.exe` fails:
"Not an ELF file - it has the wrong magic bytes at the start". So CET
IBT-markers are host-verifiable; the GNU-property note and all ELF checks
(PIE, RELRO, BIND_NOW, NX) are documented target (Linux) commands. This is a
real instance of rule 1 from the skill: the flag compiled, the binary is only
partially hardened, and readelf cannot verify the rest on this platform.

Checker runs (all actually executed):

```
python tools/hardening_audit.py good/good_readelf.txt good/good_readelf_notes.txt good/good_objdump_symbols.txt good/good_objdump_disasm.txt good/good_checksec.txt
  -> 9 protections reported; PASS (0 missing); exit 0
python tools/hardening_audit.py bad/bad_readelf.txt bad/bad_readelf_notes.txt bad/bad_objdump_symbols.txt bad/bad_objdump_disasm.txt bad/bad_checksec.txt
  -> FAIL (8 missing: PIE, RELRO partial, BIND_NOW, canary, FORTIFY, NX, IBT, SHSTK); exit 1
python tools/hardening_audit.py good/host_objdump_symbols.txt good/host_objdump_disasm.txt
  -> PASS (0 missing): canary PASS, FORTIFY PASS, CET IBT PASS, ELF-only props n/a; exit 0
python tools/hardening_audit.py bad/host_objdump_symbols.txt bad/host_objdump_disasm.txt
  -> FAIL (3 missing: canary, FORTIFY, IBT); exit 1
python tools/hardening_audit.py good/arm64_readelf.txt good/arm64_readelf_notes.txt
  -> PASS (0 missing): PAC/BTI present; exit 0
python tools/hardening_audit.py bad/arm64_readelf.txt bad/arm64_readelf_notes.txt
  -> FAIL (1 missing: PAC/BTI); exit 1
```

The `good_*`/`bad_*` ELF sample files are synthetic-but-realistic binutils-2.46
output (marked as samples in the skill), since the host produces PE binaries
that `readelf` cannot read; `host_objdump_*` are the real PE outputs above.

## Synthetic evals

- easy/negative: `bad/bad_checksec.txt` — "Partial RELRO, No canary found, NX
  disabled, No PIE, FORTIFY No" must be flagged on all columns.
- easy/negative: `bad/bad_readelf.txt` — `Type: EXEC` (no PIE) must be
  reported even when the checker never sees checksec.
- medium/negative: `bad/bad_readelf.txt` — GNU_RELRO present WITHOUT BIND_NOW
  must be reported as PARTIAL RELRO, not "RELRO ok".
- medium/positive: `good/good_readelf.txt` — `Type: DYN` + `BIND_NOW` +
  `GNU_RELRO` must be reported as PIE + full RELRO.
- hard/positive: `good/good_readelf_notes.txt` — "x86 feature: IBT, SHSTK"
  must produce both CET passes.
- hard/positive: `good/arm64_readelf_notes.txt` — "aarch64 feature: BTI, PAC"
  must produce the PAC/BTI pass and must NOT be reported as CET (n/a).
- host-positive: `good/host_objdump_*.txt` — canary symbol, `__strcpy_chk`
  call, `endbr64` marker found from real PE disassembly.

## False-positive evals (correct code must not be flagged)

- A PIE with partial RELRO is "PIE yes, full RELRO no" — never report the
  binary as fully hardened.
- A PE (Windows) binary whose build command included `-pie -Wl,-z,relro` is
  not PIE/RELRO — those properties are ELF concepts; report n/a, not "pass".
- An x86-64 ELF without a `.note.gnu.property` must report CET missing but
  PAC/BTI as n/a (ARM-only), not both missing.
- `endbr64` present but no SHSTK feature (IBT-only `-fcf-protection=branch`)
  must be "CET IBT yes, SHSTK missing", not "CET on".
- A canary symbol imported from libc (`__stack_chk_fail@@GLIBC_2.4`) in a
  stripped binary still counts as canary present — do not fail it for being an
  undefined symbol.
- `_FORTIFY_SOURCE` defined but compiled at `-O0`: no `__*_chk` calls — must
  be reported "FORTIFY off (needs -O1+)", not "FORTIFY broken".

## Historical evals

- Stack canary — stack buffer overflow class (e.g. CVE-2013-2028, nginx):
  with `-fstack-protector-strong` the corrupted canary aborts before the
  overwritten return address is used. Task: explain why the canary demo exit
  code 0xC0000409 (STATUS_STACK_BUFFER_OVERRUN) is the canary firing, and why
  an infoleak that reads the canary first defeats it (that is what CET shadow
  stack mitigates).
- FORTIFY — compiler-known-size overflows: why Heartbleed (CVE-2014-0160) was
  NOT caught by FORTIFY (the size is runtime, not compiler-known), so
  `-D_FORTIFY_SOURCE=2` is not a blanket overflow defense.
- 2016-era RELRO — Ubuntu/Debian shipped partial RELRO by default for years;
  the "Missing relro" class of binaries let GOT-overwrite turn a single write
  into control flow. Task: given a `readelf -d` without BIND_NOW, explain that
  `.got.plt` stays writable and what an attacker overwrites.
- BIND_NOW bypass scenarios — full RELRO removes the lazy-binding window and
  makes `.got` read-only, but does NOT stop overwriting other writable
  function pointers (`.fini_array`, `__free_hook`-style hooks, global vtable
  objects). Full RELRO is defense-in-depth, not control-flow integrity.
- NX — stack-shellcode class (pre-2004): with NX, injected stack code cannot
  execute; `-z execstack` (GNU_STACK RWE) silently reintroduces it. Check the
  GNU_STACK flags, never assume.
- CET / shadow stack — class of canary bypass by return-address overwrite:
  shadow stack makes the forged return address fatal even with a canary
  infoleak; requires CPU + kernel + binutils >= 2.33 support.

## Adversarial evals

- Build command claims hardening, binary does not: a Makefile that appends
  `-fstack-protector-strong` to LDFLAGS instead of CFLAGS — compiles clean,
  no canary in the binary. The agent must catch it by checking the binary.
- `-fcf-protection=full` accepted but no `.note.gnu.property` emitted (exactly
  what this host's PE toolchain does): the agent must NOT claim CET is on and
  must say how to verify on the target (readelf -n on ELF).
- `-D_FORTIFY_SOURCE=2` defined inside a `.c` file AFTER `#include <string.h>`
  — the header guards already made it a no-op; no `__*_chk` calls. The agent
  must identify the define-placement bug from the absence of `__strcpy_chk`.
- A "hardened" CI script that passes `-pie` only to the compiler (never to the
  linker): output stays ET_EXEC. The agent must demand the link command.
- A deliberately misleading checksec row claiming "Full RELRO" while the
  accompanying `readelf -d` has no BIND_NOW: the agent must trust the raw
  tool output over the summary line.

## Verification commands (target — Linux, ELF)

```
checksec --file=./app
readelf -l -d -h ./app
readelf -n ./app
objdump -t ./app | grep -E '__stack_chk_fail|__stack_chk_guard'
objdump -d ./app | grep '_chk'          # FORTIFY __strcpy_chk, __sprintf_chk...
objdump -d ./app | grep 'endbr64'       # CET IBT landing pads
readelf -W -l ./app | grep GNU_STACK    # expect 'RW ' (no E = NX)
gcc -O2 -fstack-protector-strong -fcf-protection=full -D_FORTIFY_SOURCE=2 \
    -fPIE -pie -Wl,-z,relro,-z,now -Wl,-z,noexecstack -o hardened hardened.c
```

Windows native (MSVC): `/CETCOMPAT` at link, verify with `dumpbin /headers`
(CET Compatible bit in load-config characteristics).

## Scoring

Per protection identified correctly from the evidence, 1 point. Verify-from-
binary (evidence quoted), 2 points. Correct "unknown/n-a" handling for
platform-mismatched properties (PE vs ELF, x86 vs ARM), 1 point each. Correct
"partial RELRO" nuance instead of binary pass/fail, 2 points. Total
achievable: PIE, RELRO-full, BIND_NOW, canary, FORTIFY, NX, CET-IBT, CET-SHSTK,
PAC/BTI = 9 protections x (1 + 2) + 3 nuance/n-a cases = 30 points. Pass bar:
>= 24, with zero false positives on the false-positive set. Stability on this
host: canary detection, FORTIFY redirect, IBT marker and the full checker
matrix are `source-backed` (real runs recorded above); ELF-only properties
(PIE, RELRO, BIND_NOW, NX, GNU-property note) are `target-documented` (Linux
commands + synthetic-but-realistic sample fixtures).
