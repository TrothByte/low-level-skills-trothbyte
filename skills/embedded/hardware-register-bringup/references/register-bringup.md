# Hardware Register Bring-Up — Reference Rules

Knowledge layer for `hardware-register-bringup`. Format: RULE → WHY AI
GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE →
VERIFICATION → SOURCE. Uncertainty marked KNOWN / INFERRED / UNVERIFIED.
The init-order/sequence fixtures were executed on this host (gcc, python);
target-toolchain runs (arm-none-eabi-gcc, QEMU, real silicon) are
UNVERIFIED. Relative paths assume the skill directory as CWD.

## 1. Clock enable precedes peripheral register configuration

- **RULE**: a peripheral whose clock is gated ignores register writes
  until the clock is enabled. The sequence is: bus/APB clock enable
  (e.g. RCC APB1/APB2 enable bits) → peripheral clock enable → THEN
  register configuration → peripheral enable bit last. Writing
  configuration while the clock is off is the classic "register write
  ignored" failure.
- **WHY AI GETS IT WRONG**: agents generate init code in "struct
  initialization" order (config fields first), never the clock-first
  power-on order the hardware requires.
- **CORRECT REASONING**: model the clock tree as a dependency graph:
  every register write depends on the peripheral clock being present.
  Order the writes so clock enables come first.
- **EXAMPLE** (bad): `examples/bad/init_sequence_bad.c` configures the
  peripheral registers before enabling its clock.
- **COUNTEREXAMPLE** (good): `examples/good/init_sequence_good.c` enables
  clocks first, polls ready flags, then configures.
- **VERIFICATION**: `seqbad` vs `seqgood` behavior (executed).
- **SOURCE**: stm32-ref-manual (RCC clock distribution, enable bits);
  cmsis (register names).

## 2. Reset deassert must follow clock enable and precede configuration

- **RULE**: a peripheral held in reset (reset bit set, or left at reset
  default) ignores everything until the reset is deasserted (bit
  cleared). The reset-deassert step must come after its clock is enabled
  and before register configuration; on many MCUs the RCC peripheral
  reset register defaults are the trap (peripheral left in reset).
- **WHY AI GETS IT WRONG**: agents enable the clock, configure registers,
  and never touch the reset register — the peripheral stays in reset and
  "does nothing" despite correct-looking config.
- **CORRECT REASONING**: check the reset-state of the peripheral (datasheet
  reset-value table + RCC reset register); clear the reset bit after
  clock enable, then configure.
- **EXAMPLE** (bad): `examples/bad/sequence_missing_reset.py` — clock is
  enabled but the reset bit is never deasserted.
- **COUNTEREXAMPLE** (good): `examples/good/sequence_check.py` requires
  clock → reset-deassert → config ordering.
- **VERIFICATION**: the analyzer flags the missing deassert (executed).
- **SOURCE**: stm32-ref-manual (RCC peripheral-reset registers, reset
  value tables); openocd-docs (reset/deassert at probe level).

## 3. Ready flags (HSE/PLL/oscillator) must be polled with a timeout before use

- **RULE**: clock sources are not ready instantly: HSE/PLL/PLL-lock
  ready bits are set after a settling time. Config that proceeds without
  polling these flags runs on an unstable/absent clock. Poll the flag in
  a bounded loop (never an infinite wait).
- **WHY AI GETS IT WRONG**: agents write "enable HSE, enable PLL,
  configure peripheral" with no ready checks, assuming instant lock.
- **CORRECT REASONING**: after enabling each clock source, poll its
  ready flag until set or a timeout expires; on timeout, report the
  failure instead of proceeding blind.
- **EXAMPLE** (bad): `examples/bad/init_sequence_bad.c` proceeds after
  enabling PLL without waiting for PLL_RDY.
- **COUNTEREXAMPLE** (good): `examples/good/init_sequence_good.c` polls
  with a bounded timeout.
- **VERIFICATION**: `seqbad` simulates PLL-not-ready and proceeds; the
  good fixture waits (executed).
- **SOURCE**: stm32-ref-manual (RCC clock-ready flags);
  esp-idf-docs (clock configuration order, INFERRED).

## 4. Every register field comes from the datasheet, never from memory

- **RULE**: register names, offsets, bit positions, reset values, and
  part-specific details (e.g. GPIO AF numbers) are read from the
  reference manual for the exact part number. Guessed names/offsets
  compile via CMSIS headers but target the wrong field or nothing at
  all.
- **WHY AI GETS IT WRONG**: agents recall "RCC->APB2ENR bit 4 = USART1"
  from a different part or from memory and write a bit that enables a
  different (or nonexistent) clock.
- **CORRECT REASONING**: for each register written, confirm the name in
  the part's headers, the offset/field from the reference manual, and
  the reset value; verify with a compile-time static assert where
  possible.
- **EXAMPLE** (bad): writing `(1 << 4)` to an APB-enable register without
  checking which peripheral bit 4 actually gates on this part.
- **COUNTEREXAMPLE** (good): `examples/good/rcc_init.c` names the bits
  from the datasheet and static-asserts the offsets.
- **VERIFICATION**: static asserts compile; datasheet cross-check
  documented. Target build UNVERIFIED.
- **SOURCE**: stm32-ref-manual (register maps); cmsis (part-specific
  headers).

## 5. Read-modify-write only documented fields; never guess reserved bits

- **RULE**: when a register has a known reset value, either write the
  full value from the datasheet or use RMW that sets/clears only the
  documented fields. Reserved bits must be left at their reset value —
  writing guessed values to them can enable test modes or undefined
  behavior.
- **WHY AI GETS IT WRONG**: agents RMW with a mask assembled from
  "probably" bits, or OR in a reserved field by mistake.
- **CORRECT REASONING**: derive the mask from the datasheet field
  table; verify reserved fields are untouched by the mask.
- **EXAMPLE** (bad): OR-ing bit 6 (reserved) into a GPIO config register.
- **COUNTEREXAMPLE** (good): field-masks built only from documented bits,
  with the datasheet row cited.
- **VERIFICATION**: mask audit vs datasheet field table; host-runnable
  mask logic in the good fixture.
- **SOURCE**: stm32-ref-manual (field tables, reset values);
  embedded-hw-register-datasheet-verification pattern.

## 6. Peripheral enable bit is the LAST step, after clock, reset, and config

- **RULE**: peripherals typically gate themselves on an enable bit
  (e.g. USART_CR1.UE, SPI_CR1.SPE). The enable must come after clock,
  reset-deassert, and all configuration; enabling early can cause the
  peripheral to act on partially-configured state (e.g. a UART sending
  with wrong baud) or reject late config writes.
- **WHY AI GETS IT WRONG**: agents set the enable bit at the top of the
  init function out of "turn it on first" intuition.
- **CORRECT REASONING**: configuration before enable; the enable bit is
  the last write in the sequence.
- **EXAMPLE** (bad): enabling the UART before setting baud/parity.
- **COUNTEREXAMPLE** (good): config complete, then enable as final step.
- **VERIFICATION**: order assertion in the sequence analyzer (executed).
- **SOURCE**: stm32-ref-manual (peripheral control registers);
  esp-idf-docs (INFERRED for ESP32 parts, verify against the part).

## Quick reference table

| Step | Rule |
|---|---|
| supply ramp | wait for power-good / min time (datasheet) |
| reset deassert | after clock source ready; before config |
| clock source | HSE/PLL enable + ready-poll with timeout |
| bus clocks | AHB→APB enables for the peripheral bus |
| peripheral clock | RCC peripheral enable bit |
| peripheral reset | clear the RCC reset bit (after clock) |
| configuration | every field from the datasheet, RMW documented bits |
| enable bit | LAST, after all configuration |
