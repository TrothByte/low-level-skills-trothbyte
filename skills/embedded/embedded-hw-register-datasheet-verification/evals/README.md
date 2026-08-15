# Evaluation — embedded-hw-register-datasheet-verification

Skill: `skills/embedded/embedded-hw-register-datasheet-verification`.
Toolchain: gcc 16.1.0 (MSYS2 ucrt64, host x86_64, target `x86_64-w64-mingw32`,
PE/COFF). The register model is host-compiled; `_Static_assert` and
`offsetof` are target-independent, so the compile-time checks are valid for
any target the model is later compiled for. No cross compiler was used; the
model itself is the "device" the examples talk to.

## Synthetic evals (SOURCE-BACKED, run 2026-08-15)

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/negative | `bad/i2c_wrong_bit.c` | compile error, TxE != 0x80 | exit 1 |
| easy/negative | `bad/clock_enable_wrong.c` | compile error, I2C1EN is bit 21 | exit 1 |
| medium/negative | `bad/madctl_swapped_bits.c` | compile error, claimed value != datasheet bits | exit 1 |
| medium/negative | `bad/i2c_wrong_family.c` | compile error, SR1 is at 0x14 not 0x18 | exit 1 |
| medium/negative | `bad/reset_value_wrong.c` | compile error, TRISE resets to 0x0002 | exit 1 |
| positive | `good/madctl_bits.c` | compiles, MADCTL = 0x68, no reserved bits | exit 0 |
| positive | `good/i2c_register_map.c` | compiles, F1-only SR1/RCC bit 21 | exit 0 |

Recorded assertion texts (each `gcc -std=c11 -c` exits 1):

```
bad/madctl_swapped_bits.c: error: static assertion failed:
  "claimed MADCTL value does not match the datasheet bit table"
bad/i2c_wrong_family.c: error: static assertion failed:
  "SR1 is at 0x14 per RM0008; 0x18 is SR2"
bad/i2c_wrong_bit.c: error: static assertion failed:
  "TxE is bit 6 (0x40); 0x80 is BERR"
bad/clock_enable_wrong.c: error: static assertion failed:
  "I2C1EN is bit 21 per RM0008 RCC_APB1ENR"
bad/reset_value_wrong.c: error: static assertion failed:
  "TRISE reset is 0x0002 per the RM0008 reset table"
```

## Verified facts (ACTUAL program output, exit 0)

`gcc examples/good/runtime_offset_dump.c && ./a.out` printed:

```
I2C_TypeDef size=0x24
I2C1 base=0x40005400
SR1 offset=0x14 (expected 0x14)
SR2 offset=0x18 (expected 0x18)
TRISE offset=0x20 (expected 0x20)
TRISE reset=0x0002 (expected 0x0002)
SR1 TxE mask=0x0040 (expected 0x0040)
SR1 defined bits=0xDF7F (expected 0xDF7F)
I2C1EN bit=21 (expected 21)
MADCTL mask=0xFC (expected 0xFC)
landscape+BGR MADCTL=0x68 (expected 0x68)
MADCTL reset=0x00 (expected 0x00)
```

Notes on the model build: the first draft of the stub header carried an
arithmetic bug (SR1 defined-bits mask 0xFBFF, MADCTL mask 0xF0) that the
`_Static_assert` machinery itself rejected at compile time — i.e. the model
reproduced the exact failure mode it teaches. After correction the asserts
pass; the corrected values (0xDF7F, 0xFC) are the datasheet-consistent ones.

## False-positive evals (correct code must NOT be flagged)

- `good/madctl_bits.c` — MX/MV chosen from the rotation intent, BGR set,
  reserved bits absent.
- `good/i2c_register_map.c` — F1-family SR1 constants, RCC_APB1ENR bit 21,
  START/ACK on CR1.
- A driver for STM32F0 that (correctly) uses `ISR`/`TXIS` must NOT be flagged
  as wrong when the target part is stated as STM32F0 — family identity is the
  deciding factor, not the register name.
- `MADCTL = 0x68` must not be flagged as "wrong" just because another rotation
  value exists; the intended rotation is part of the spec.

## Adversarial evals (researched — no target hardware available)

- A cross-family driver that compiles against a *hypothetical* combined
  header (F1 CR1 + F0 ISR) — must be rejected by cross-checking against the
  two real headers. Toolchain: needs `stm32f1xx.h`/`stm32f0xx.h` (CMSIS
  Device Family Pack) + a real build; recorded as researched.
- A MADCTL comment lying about intent ("portrait") while the byte is 0x68
  (landscape) — caught by re-deriving the byte from the stated intent, not by
  the compiler. Reviewed manually; no host tool automates intent.

## Historical evals

- Not applicable yet: no curated bug database of hallucinated register maps
  is registered. Candidate: GHSA-style reports on bogus STM32 drivers and
  the "Cursor STM32 I2C nonexistent-register" class. Status: UNVERIFIED.

## Target toolchains (absent, documented)

- No STM32 cross compiler (arm-none-eabi-gcc) on this host — the compile
  checks are host-side `_Static_assert` models; a real target build is the
  final gate before flashing.
- No device/simulator (no QEMU with an STM32 model) — runtime register
  behavior on hardware is UNVERIFIED here.
