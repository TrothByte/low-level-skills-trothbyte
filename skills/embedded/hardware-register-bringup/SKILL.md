---
name: hardware-register-bringup
description: Use when bringing up unknown MCU/peripheral registers from a datasheet: power-on reset sequences, clock enabling and reset deassert order, register init-order reasoning, and datasheet-first validation before writing firmware. Teaches the power-on-to-peripheral-ready sequence and how to verify register values against the reference manual before flashing.
---
# Hardware Register Bring-Up: Power-On Reset and Init Sequences

## When to use

- Bringing up a new MCU or peripheral from the reference manual:
  first UART, GPIO, clock tree, watchdog, or power domain.
- Deriving the exact power-on reset sequence: supply ramp, reset
  deassert, clock enable (PLL/oscillator), bus/peripheral clock gates,
  peripheral reset release, then register configuration.
- Writing init code for an MCU/peripheral you have not used before and
  must verify against the datasheet register table.
- Reviewing generated (LLM) init code for init-order and clock/reset
  mistakes before flashing hardware.

## When not to use

- Already-working peripheral code on a supported vendor HAL — use the HAL
  (CMSIS/ESP-IDF/Zephyr) instead of hand-rolled registers.
- General volatile/MMIO ordering semantics — use
  `embedded-volatile-and-memory-ordering`.
- Verifying that a *specific* register bit value matches the datasheet
  (already-registered code) — use
  `embedded-hw-register-datasheet-verification`.
- Debugging a running system with a debugger — use
  `embedded-flash-debug-cycle`.

## What the agent often gets wrong

- Writing peripheral register config BEFORE enabling the peripheral's
  clock: writes to a clock-gated peripheral silently do nothing (the
  "I wrote the register but it doesn't work" class).
- Releasing a peripheral from reset (setting a reset bit to 0) before
  its clock is enabled, or never deasserting reset — the peripheral
  stays in reset and never responds.
- Enabling the wrong clock: enabling the peripheral clock but not the
  bus clock / PLL / oscillator it depends on, or enabling a clock gate
  on the wrong RCC register/bit.
- Not waiting for clock-ready flags: proceeding to peripheral config
  while the PLL/oscillator is still locking (HSE_RDY/PLL_RDY poll
  missing), so the clock is unstable during config.
- Inventing register names/bit positions from memory instead of the
  datasheet ("RCC->APB2ENR |= (1<<4)" guessed, not verified) — compiles
  via CMSIS, silently wrong.
- Configuring power/peripheral state with a read-modify-write to a
  register whose reset value the agent didn't check (reserved bits
  written as 1, or a field left in an unintended reset state).
- Assuming reset sequence "just works": no explicit deassert for a
  peripheral held in reset by a default reset-state register, and no
  minimum-time wait for supplies/oscillators.

## How to reason correctly

1. **Datasheet-first for every register**: name, offset, reset value,
   field bits, and the part-specific RCC/GPIO/clock tree come from the
   reference manual for the EXACT part number. CMSIS headers provide
   names, not guarantees that a name/bit exists for your part.
2. **Order the power-on sequence**:
   power/supply ramp → reset deassert (nRST / SYSRESET, wait release)
   → clock source (HSI/HSE/PLL, poll ready flags) → bus clocks
   (AHB→APB1/APB2 enables) → peripheral clock enable → peripheral reset
   deassert (RCC peripheral-reset bit cleared) → register config →
   peripheral enable bit LAST. Never config-then-clock.
3. **Every enabled peripheral has a clock AND a deasserted reset**: check
   both; a peripheral in reset ignores everything, and one with no clock
   ignores everything too. Verify the reset-state bit before deassert.
4. **Poll ready flags with a timeout**: HSE/PLL/oscillator-ready bits are
   polled (never assumed instantly ready); use a bounded loop, not an
   infinite wait.
5. **Prefer whole-register writes from the datasheet reset value, then
   set fields**: when the reset value is known, write it (or use
   read-modify-write on only the documented fields); never guess
   reserved bits.
6. **When unsure, re-read the datasheet**: search the reference manual
   for the register/bit before writing code; the init-order check and the
   register table are the ground truth.

## What to verify

- Every register written exists in the datasheet for the exact part; no
  invented names/bits.
- Init order is power → reset-deassert → clock-ready → bus clocks →
  peripheral clock → peripheral reset-deassert → config → enable.
- Ready flags (HSE/PLL/oscillator) are polled with a timeout before
  proceeding.
- The peripheral's reset-deassert happens after its clock is enabled;
  no peripheral is configured while still in reset or clock-gated.
- RMW operations touch only documented fields; reserved bits are not
  assumed.
- Host-runnable simulation of the sequence logic passes (order + timeout
  checks).

## How to verify

```
# Host-verifiable: init-order and sequence-logic simulation (executed)
gcc -Wall -Wextra -Werror -O2 examples/good/init_sequence_good.c -o seqgood
seqgood                         # PASS: order + ready-poll + timeout correct
gcc -Wall -Wextra -Werror -O2 examples/bad/init_sequence_bad.c -o seqbad
seqbad                          # BAD: config-before-clock / no ready poll

python examples/good/sequence_check.py     # PASS: order analyzer
python examples/bad/sequence_missing_reset.py  # FAIL: reset never deasserted

# Host compile-check of the datasheet-derived register stub (executed):
gcc -Wall -Wextra -Werror -O2 -c examples/good/rcc_init.c   # static asserts pass

# Target (documented; arm-none-eabi-gcc / QEMU not installed here):
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -c examples/good/rcc_init.c
# or QEMU: qemu-system-arm -machine netduino2  (STM32F405 model)
```

The init-order/sequence logic is host-verifiable (gcc + python runs,
recorded in `evals/README.md`); register values must be checked against
the datasheet (stm32-ref-manual), and the target toolchain is UNVERIFIED
here.

## Where the knowledge comes from

- `stm32-ref-manual` — RCC/GPIO/EXTI register maps, reset/clock
  distribution, ready flags.
- `cmsis` — header names and the part-specific limits of those names.
- `devicetree-spec` — where the reset/clock order is encoded in DT
  properties on DT-driven platforms.
- `openocd-docs` — reset/deassert sequences at the debug-probe level.
- `esp-idf-docs` — power/reset/clock ordering on ESP32-class parts
  (INFERRED detail, verify against the part datasheet).

## Related skills

- `embedded-hw-register-datasheet-verification` (require; verify each
  register bit against the datasheet).
- `embedded-volatile-and-memory-ordering` (require; why register writes
  must be volatile and ordered).
- `embedded-board-bringup-peripheral-init` (recommend; adjacent bring-up
  checklist).
- `embedded-flash-debug-cycle` (recommend; flashing and debugger
  verification of the sequence).
- `embedded-interrupt-and-nested` (recommend; NVIC enable order after
  peripheral init).

## Evaluation

- Synthetic: `bad/init_sequence_bad.c` (config before clock, no ready
  poll) and `bad/sequence_missing_reset.py` (reset never deasserted)
  must be flagged; `good/init_sequence_good.c` and
  `good/sequence_check.py` must pass.
- False-positive: a peripheral with no reset register (always running)
  legitimately has no deassert step; polling a ready flag with a timeout
  is correct, not "wasted".
- Historical: config-before-clock and stuck-in-reset bugs are documented
  embedded failure classes (mcuoneclipse/EE community write-ups);
  UNVERIFIED as named incidents on this host.
- Adversarial: code that "works" in a simulator because the simulator
  ignores clock/reset gating, but does nothing on silicon; a guessed
  register bit that compiles via CMSIS but does not exist for the part.
- Verified facts and commands: `evals/README.md`.
