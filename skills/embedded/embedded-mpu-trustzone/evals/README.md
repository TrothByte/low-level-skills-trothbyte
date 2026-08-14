# Evaluation — embedded-mpu-trustzone

Skill: `skills/embedded/embedded-mpu-trustzone`. Stability target: `evaluated`
(host-verified; target verification needs an ARM toolchain + QEMU/hardware,
which this repository's x86-only toolchain cannot run).

## Verified facts (this environment, 2026-08-14)

- Toolchain: `gcc.exe (Rev5, Built by MSYS2 project) 16.1.0`, x86_64 MinGW —
  no ARM backend.
- Register layouts verified against CMSIS-5.9 headers (`core_cm4.h`,
  `core_cm33.h`, `mpu_armv7.h`, `mpu_armv8.h`):
  - ARMv7-M RASR: XN=28, AP=26:24, TEX=21:19, S=18, C=17, B=16, SRD=15:8,
    SIZE=5:1, ENABLE=0; RBAR: ADDR=31:5, VALID=4, REGION=3:0.
  - ARMv8-M RBAR: BASE=31:5, SH=4:3, AP=2:1, XN=0; RLAR: LIMIT=31:5,
    AttrIndx=4:1, EN=0; MAIR0/MAIR1 at offsets 0x030/0x034 (8-bit slots,
    index 0-7).
  - SAU: RBAR BADDR=31:5, RLAR LADDR=31:5, NSC=1, ENABLE=0;
    CTRL ALLNS=1, ENABLE=0. No S bit in RLAR: regions are NS or NSC;
    the SAU default is Secure.
- ARM target flag on the host compiler is unsupported (recorded output below).
- Executable gate on host: `-Wall -Wextra -Werror -O2` + runtime asserts.

## Synthetic evals

Each case: DETECT (find the defect) -> EXPLAIN (name the rule) -> FIX
(apply the reference rule) -> VERIFY (host asserts + target command).

| Level | Fixture / case | Defect to detect | Rule |
|---|---|---|---|
| easy | `mpu_set_region` writes RBAR/RLAR without EN | missing per-region enable | 1.2 / 2.1 |
| easy | `MPU.CTRL` never written (or no ENABLE) | MPU-level enable missing | 1.2 |
| medium | SRAM region base 0x20002000, size 0x20000 | base not aligned to size | 1.1 / 2.2 |
| medium | region size 0x30000 (not power of two) | non-pow2 size | 1.1 / 2.2 |
| medium | `MPU.CTRL = ENABLE` without PRIVDEFENA while code relies on unmapped privileged access | background-region policy wrong | 1.3 |
| hard | SAU never configured before NS handover | everything stays Secure; NS image faults | 3.1 / 3.2 |
| hard | veneer region marked Non-secure (NSC=0) | NS-to-S entry broken | 3.3 |
| hard | secure->NS call via plain function pointer, no `cmse_nonsecure_call` | callee runs in Secure state | 3.4 |
| hard | UART region with a cacheable Normal MAIR slot, MMIO read via non-volatile pointer | stale reads under -O2 | 3.7 |
| adversarial | host model passes all asserts, but the ARM build command omits `-mcmse`: the `cmse_nonsecure_call` attribute is silently ignored and the disassembly shows BLX, not BLXNS | toolchain flag missing | 3.4 |
| adversarial | MPU regions look valid but the SAU region addresses disagree with the linker script (e.g. NS app linked to an address the SAU marks Secure) | partition mismatch | 3.6 |

## False-positive evals (correct code must NOT be flagged)

- The `examples/good` setup — all region descriptors valid, EN and
  PRIVDEFENA set, SAU enabled with an NSC region, Device MAIR slot for the
  UART — must pass every check (host gate exits 0).
- A correctly aligned 64 KB region at base 0x20000000 must not be flagged as
  misaligned.
- PRIVDEFENA=1 must not be flagged when the design intentionally allows
  privileged access to unmapped peripherals via the default map.
- ARMv7-M RBAR/RASR code must not be flagged for "missing MAIR" — MAIR does
  not exist on ARMv7-M (see reference rule 1.5).
- A secure image with `cmse_nonsecure_call` AND `-mcmse` in the build must not
  be flagged for the NS-call rule.
- An over-permissive AP (full access everywhere) is a hardening note, not an
  ALWAYS-FLAG error.

## Verification commands (as actually run, this environment)

```
> gcc --version
gcc.exe (Rev5, Built by MSYS2 project) 16.1.0

> gcc -Wall -Wextra -Werror -O2 -c examples/good/mpu_trustzone_good.c -o %TEMP%\tz_good.o
exit 0                                          # good compiles clean

> gcc -Wall -Wextra -Werror -O2 -c examples/bad/mpu_trustzone_bad.c -o %TEMP%\tz_bad.o
exit 0                                          # bad ALSO compiles clean (the trap)

> gcc -Wall -Wextra -Werror -O2 examples/good/mpu_trustzone_good.c -o %TEMP%\tz_good.exe
> %TEMP%\tz_good.exe
exit 0                                          # all invariant asserts pass

> gcc -Wall -Wextra -Werror -O2 examples/bad/mpu_trustzone_bad.c -o %TEMP%\tz_bad.exe
> %TEMP%\tz_bad.exe
Assertion failed: region_ok(0x20002000U, 0x00020000U),
  file examples/bad/mpu_trustzone_bad.c, line 145
exit -1073740791 (0xC0000409)                   # abort on first failed invariant (B1)

> gcc -mcpu=cortex-m33 -mthumb -c examples/good/mpu_trustzone_good.c
warning: '-mcpu=' is deprecated; use '-mtune=' or '-march=' instead
error: unrecognized command-line option '-mthumb'
exit 1
# Expected in this repo: the x86-only MinGW gcc has no ARM backend,
# so ARM-target compilation fails at option parsing ("invalid target").
# Use arm-none-eabi-gcc for the real ARM build.
```

## Target verification (ARM toolchain + QEMU or hardware)

```
arm-none-eabi-gcc -mcpu=cortex-m33 -mthumb -mcmse -Wall -Wextra -Werror -O2 \
  -c examples/good/mpu_trustzone_good.c
arm-none-eabi-objdump -d image.elf | grep -i blxns    # expect BLXNS on NS calls
arm-none-eabi-objdump -d image.elf | grep -i "e97f"   # expect SG in .gnu.sgstubs

qemu-system-arm -machine mps2-an505 -cpu cortex-m33 -nographic   # ARMv8-M TZ
qemu-system-arm -machine mps2-an385 -cpu cortex-m3  -nographic   # ARMv7-M MPU
```

On target, expect: no SecureFault in `SAU_SFSR`/`SFAR` after NS boot, NS
interrupts deferred while in Secure state, and NS code that touches Secure
memory faulting as configured.

## Scoring (for routing eval)

- precision: every flagged defect must map to a reference rule (1.x-3.7).
- recall: the easy/medium/hard fixtures above must all be detected.
- FP-rate: the false-positive fixtures must produce zero flags.
