---
name: embedded-hw-register-datasheet-verification
description: Use when writing or reviewing embedded peripheral drivers that touch raw registers — GPIO, I2C, SPI, display controllers, clocks. Prevents hallucinated registers, swapped bit positions, wrong reset values, and cross-family register maps by encoding the datasheet layout as a compilable C model.
---

# Hardware Register Datasheet Verification

## When to use

- Writing or reviewing code that reads/writes memory-mapped peripheral registers
  (GPIO, I2C, SPI, UART, DMA, RCC, display controllers like ST7789).
- Diagnosing "beautiful code that doesn't work" — device status never appears,
  interrupts never fire, screens stay blank.
- Porting a driver between chip families (STM32F0 vs STM32F1, HAL vs bare metal).
- Reviewing LLM-generated driver code where register names look plausible.

## When not to use

- Board-level wiring, schematics, or PCB questions — no registers involved.
- Zephyr/DeviceTree where hardware is described by `compatible` strings and
  Kconfig — use `embedded-device-tree-and-kconfig`.
- Application-level logic that never touches hardware registers.
- Code already written against a vendor HAL with proven compile-time types
  (CMSIS structs) — there the type system already checks offsets; verify only
  bit semantics and reset values against the datasheet.

## What the agent often gets wrong

- Hallucinating registers that do not exist on the part (`I2C1->ISR` on an
  STM32F1, which has `SR1`/`SR2`, not `ISR`).
- Blending two families: F0 I2C status bits (TXIS/NACKF) into an F1 driver
  that polls SR1, so the wait loop watches the wrong meaning.
- Swapping MADCTL bit meanings from memory (MV=0x20 vs MX=0x40) and shipping
  a display in the wrong orientation with a "confident" comment.
- Writing reserved bits (MADCTL D1:D0, SR1 D13) because the mask "should cover
  the register".
- Guessing reset values ("everything resets to 0") — I2C_TRISE resets to 0x0002.
- Getting the clock enable bit wrong (I2C1EN is RCC_APB1ENR bit 21, not bit 1),
  so the peripheral is never clocked and every poll hangs.

## How to reason correctly

1. Name the exact part number and open the family-specific register table
   (SoC reference manual + CMSIS header) before writing any constant.
2. Treat every register, bit, and reset value as a claim with a datasheet
   location, and encode the layout as a C struct + macros.
3. Add `_Static_assert` checks for offsets (`offsetof`), block size
   (`sizeof`), defined-bit union, and reset values — the compiler becomes the
   datasheet comparison.
4. Mask reserved bits on every read-modify-write.
5. Trace the enable chain: RCC clock gate bit → GPIO mode/AF → peripheral
   register. A driver without all three links is incomplete.
6. When in doubt about a value, write the datasheet table name into the code
   as a comment near the constant and re-verify by compiling the model.

## What to verify

- Every register offset and bit mask has a datasheet-table provenance.
- The defined-bit union of the register equals the datasheet mask (no reserved
  bits set).
- Reset values used in init guards match the reset table.
- Clock enable bit and pin mux are named and asserted.
- The driver targets one specific family; no cross-family constants leak in.
- The C register-map model compiles clean under `-Wall -Wextra -Werror`.

## How to verify

```
gcc -std=c11 -Wall -Wextra -Werror -c examples/good/madctl_bits.c
gcc -std=c11 -Wall -Wextra -Werror -c examples/good/i2c_register_map.c
gcc -std=c11 -c examples/bad/madctl_swapped_bits.c     # exit 1
gcc -std=c11 -c examples/bad/i2c_wrong_family.c        # exit 1
gcc -std=c11 -c examples/bad/i2c_wrong_bit.c           # exit 1
gcc -std=c11 -c examples/bad/clock_enable_wrong.c      # exit 1
gcc -std=c11 -c examples/bad/reset_value_wrong.c       # exit 1
gcc examples/good/runtime_offset_dump.c && ./a.out
```

A bad file exits 1 with the assertion text naming the violated rule; the dump
prints the datasheet-derived offsets, masks, and reset values.

## Where the knowledge comes from

- `st7789-datasheet` — ST7789V command set: MADCTL (0x36) bit table, reserved
  D1:D0, reset value.
- `stm32-ref-manual` — STM32F1 I2C register map (CR1..TRISE), I2C_SR1 bit
  table, reset column, RCC_APB1ENR (I2C1EN bit 21).
- `cmsis` — stm32f1xx/stm32f0xx header structs; cross-family differences.

## Related skills

- `embedded-volatile-and-memory-ordering` — volatile access and MMIO ordering
  around register writes.
- `embedded-device-tree-and-kconfig` — same verification discipline for
  Zephyr/DeviceTree boards instead of raw registers.
- `embedded-linker-script`, `embedded-interrupt-and-nested` — register maps
  feed interrupt configuration and startup code.
- `c-undefined-behavior` — packed/volatile struct rules that apply to
  register structs.

## Evaluation

- Synthetic: each `examples/bad/*.c` must fail to compile with a
  `_Static_assert` message naming the datasheet rule; each good file must
  compile with exit 0.
- False-positive: correct drivers — MX/MV chosen from the rotation intent,
  F1-only SR1 constants, RCC bit 21, TRISE reset 0x0002 — must pass unmodified.
- Adversarial: a plausible-looking cross-family driver (F0 ISR constants in an
  F1 init sequence) must be rejected; a "correct-looking" wrong MADCTL value
  (0x69 with a comment claiming 0x68) must be caught.
- Verified facts: actual gcc 16.1.0 runs recorded in `evals/README.md`
  (exit codes + assertion texts + dump output).
