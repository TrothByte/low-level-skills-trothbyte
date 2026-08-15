# Hardware Register Datasheet Verification — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE(bad) →
COUNTEREXAMPLE(good) → VERIFICATION → SOURCE. Source ids refer to
registry/sources.yaml. Status tags: KNOWN = verified against the datasheet
text this skill cites; INFERRED = from secondary sources, re-check on target.

## 1. Register names, bit positions, and reset values come from the datasheet, not from memory

- **RULE**: every register offset, bit position, and reset value an agent
  writes must trace to a specific table in the peripheral datasheet or the
  SoC reference manual. "Pretty names" that sound right (SR1, MADCTL, TXIS)
  are not evidence. Datasheet tables are the only authority; other people's
  code and LLM memory are hints to verify, not sources.
- **WHY AI GETS IT WRONG**: models interpolate register maps. They merge the
  STM32F0 I2C (I2C_ISR at 0x18, TXIS/RXNE/NACKF bits) with the STM32F1 I2C
  (I2C_SR1 at 0x14, SB/ADDR/TxE/BERR bits), or swap ST7789 MADCTL bits
  (MY=0x80, MX=0x40, MV=0x20) and silently reuse a "reset 0x00" that some
  registers do not have. Result: code that reads the wrong meaning and
  "works" only by coincidence or not at all.
- **CORRECT REASONING**: treat every register constant as a claim. Assign it
  a datasheet location (RM0008 §22.6, ST7789V command table). Encode the
  layout as a C struct + `_Static_assert`, so the compiler rejects any value
  that disagrees with the documented table. Name the source for each field.
- **EXAMPLE** (bad):
  ```c
  /* F0 tutorial constants dropped into an F1 driver */
  while ((I2C1->ISR & I2C_ISR_TXIS) == 0u) { }  // F1 has no ISR, no TXIS
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  _Static_assert(offsetof(I2C_TypeDef, SR1) == 0x14u, "SR1 at 0x14 per RM0008");
  while ((I2C1->SR1 & I2C_SR1_TxE) == 0u) { }      /* TxE = bit 6 = 0x40 */
  ```
- **VERIFICATION**: `gcc -std=c11 -Wall -Wextra -Werror -c` — a wrong
  constant fails `_Static_assert` with the claim text. Recorded 2026-08-15:
  all five `examples/bad/*.c` exit 1 with the message naming the rule.
- **SOURCE**: st7789-datasheet (MADCTL command table); stm32-ref-manual
  (I2C register map, reset table); cmsis (stm32f1xx bit definitions).

## 2. Reserved bits are read-only and must stay at their reset value

- **RULE**: ST7789 MADCTL D1:D0 are reserved; writing them is undefined.
  STM32F1 I2C_SR1 D13 is reserved; software must treat it as zero and never
  clear/assert it. Reserved-bit writes look innocent in review but can set a
  test-mode or change future-part behavior.
- **WHY AI GETS IT WRONG**: agents define a mask that "covers the register"
  and OR in flags across the whole 16/32-bit field, or they fill unused bits
  with 1s "for safety". The reserved region is exactly where a model guesses
  and corrupts.
- **CORRECT REASONING**: mask out reserved bits before every read-modify-write:
  `value = (value & 0xFFFCu) | chosen_bits;`. Verify with a compile-time check
  that the final byte/word contains no set bit outside the defined mask.
- **EXAMPLE** (bad):
  ```c
  uint8_t madctl = 0x68u | 0x01u;   /* reserved D0 forced on */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  _Static_assert((0x68u & ~MADCTL_MASK) == 0u, "no reserved bit set");
  uint8_t madctl = (uint8_t)((0x68u & MADCTL_MASK) | 0u);
  ```
- **VERIFICATION**: `_Static_assert` in the good example passes (exit 0);
  `bad/madctl_swapped_bits.c` asserts `0x69 == 0x68` and fails with
  "claimed MADCTL value does not match the datasheet bit table".
- **SOURCE**: st7789-datasheet (MADCTL reserved D1:D0); stm32-ref-manual
  (I2C_SR1 D13 reserved, read as 0).

## 3. Clock enable and GPIO mux bits are part of the register map

- **RULE**: a peripheral register map is unusable without its clock gate and
  pin mux. STM32F103: I2C1 clock = RCC_APB1ENR bit 21 (I2C1EN); default pins
  PB6=SCL, PB7=SDA configured as alternate-function open-drain. Enabling the
  wrong APB bit starts a different peripheral and leaves the target clocked off.
- **WHY AI GETS IT WRONG**: the clock/pin layer is in a different table than
  the peripheral itself, so models often skip it or guess the bit (e.g. bit 1,
  which is TIM2). The result runs nothing and hangs on the first status poll.
- **CORRECT REASONING**: for every driver, list (a) the RCC enable bit for
  the peripheral and bus, (b) the GPIO pins + mode + AF/remap, and (c) a
  compile-time check of the enable bit value. If the pin is remappable, name
  the AFIO_MAPR bit and verify it from the manual, not from memory.
- **EXAMPLE** (bad):
  ```c
  #define I2C1EN_CLAIMED (1u << 1)  /* enables TIM2, not I2C1 */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  #define RCC_APB1ENR_I2C1EN (1u << 21)
  _Static_assert((1u << 21) == RCC_APB1ENR_I2C1EN, "I2C1EN is bit 21");
  ```
- **VERIFICATION**: `bad/clock_enable_wrong.c` fails with "I2C1EN is bit 21
  per RM0008 RCC_APB1ENR"; `good/i2c_register_map.c` compiles clean and its
  enable function writes `(RCC_BASE + 0x1C)` |= bit 21.
- **SOURCE**: stm32-ref-manual (RCC_APB1ENR table, AFIO_MAPR, GPIO AF modes);
  st7789-datasheet (SPI CS/DC/RESET handling is board-level, not register).

## 4. Same-named peripherals differ between families — confirm the family first

- **RULE**: STM32F1 I2C (CR1/CR2/OAR1/OAR2/DR/SR1/SR2/CCR/TRISE, status via
  SR1 flags SB/ADDR/TxE/RxNE) and STM32F0 I2C (CR1/CR2/OAR1/OAR2/TIMINGR/
  TIMEOUTR/ISR/ICR/PECR/RXDR/TXDR, status via ISR flags TXIS/RXNE/ADDR/NACKF)
  are different peripherals. The F0 HAL has no `I2C_SR1`; the F1 HAL has no
  `I2C_ISR`. Writing one family's registers into the other's driver is the
  classic "beautiful code that doesn't work".
- **WHY AI GETS IT WRONG**: training data mixes family examples; the model
  produces a plausible union of both layouts, so `I2C1->ISR` compiles in the
  agent's imagination but not against any real header.
- **CORRECT REASONING**: before writing a single register, name the exact
  part and open the family-specific CMSIS struct. Cross-family constants are
  automatically suspect. Put `_Static_assert(offsetof(...))` checks on the
  registers you poll so a family mismatch is a compile error.
- **EXAMPLE** (bad):
  ```c
  /* F0-style access applied to F1: 0x18 is SR2, not an ISR */
  while ((*(volatile uint32_t *)(I2C1_BASE + 0x18u) & 0x08u) == 0u) { }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  _Static_assert(offsetof(I2C_TypeDef, SR1) == 0x14u, "F1 SR1");
  while ((I2C1->SR1 & I2C_SR1_ADDR) == 0u) { }
  ```
- **VERIFICATION**: `bad/i2c_wrong_family.c` fails with "SR1 is at 0x14 per
  RM0008; 0x18 is SR2" (the F0 ISR offset asserted against the F1 model).
- **SOURCE**: stm32-ref-manual (F1 I2C register map); cmsis (stm32f1xx vs
  stm32f0xx struct layouts differ).

## 5. Reset values matter for init guards and debug comparisons

- **RULE**: use the datasheet reset table, not the assumption "everything is
  0". STM32F103 I2C_TRISE resets to 0x0002 (rise time in clock cycles).
  ST7789 MADCTL resets to 0x00. A "reset check" that compares against a
  fabricated 0x0000 for TRISE will never match a freshly reset peripheral.
- **WHY AI GETS IT WRONG**: "reset value 0" is the model's default guess;
  nonzero reset values are remembered only when a bug surfaced around them.
- **CORRECT REASONING**: read the reset column of the register table. If you
  rely on a value after reset, encode the documented value as a constant and
  assert it. Keep the reset value next to the register definition so the two
  are reviewed together.
- **EXAMPLE** (bad):
  ```c
  if (I2C1->TRISE != 0x0000u) { /* "verify reset" — always false */ }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  _Static_assert(I2C_TRISE_RESET == 0x0002u, "TRISE reset 0x0002 per RM0008");
  ```
- **VERIFICATION**: `bad/reset_value_wrong.c` fails with "TRISE reset is
  0x0002 per the RM0008 reset table"; `examples/good/runtime_offset_dump.c`
  prints `TRISE reset=0x0002`, `MADCTL reset=0x00`, `SR1 TxE mask=0x0040`,
  `SR1 defined bits=0xDF7F`, `MADCTL mask=0xFC` (actual program output).
- **SOURCE**: stm32-ref-manual (I2C registers reset column); st7789-datasheet
  (MADCTL reset value in the command table).

## 6. Layout arithmetic: struct offsets, masks, defined-bit coverage

- **RULE**: the register struct must satisfy offset invariant checks
  (SR1 at 0x14, block size 0x24) and the union of defined bits must equal the
  datasheet mask (SR1 = 0xDF7F: D0..D12, D14, D15; D13 reserved). Mistakes
  here are silent: the code reads a shifted bit and misinterprets status.
- **WHY AI GETS IT WRONG**: bit arithmetic is done by eye; agents write
  `0xFBFF`-style masks and never recompute the OR of the actual bit defines.
- **CORRECT REASONING**: define bits as masks, then OR them and compare with
  the datasheet mask in a `_Static_assert`. Let the compiler do the
  arithmetic. The same pattern works for `offsetof` and `sizeof`.
- **EXAMPLE** (bad): a mask `0xFBFF` claimed as "SR1 defined bits" while the
  real union is `0xDF7F`.
- **COUNTEREXAMPLE** (good):
  ```c
  _Static_assert((I2C_SR1_SB | ... | I2C_SR1_SMBALERT) == 0xDF7Fu,
                 "SR1 defined bits D0..D12,D14,D15");
  ```
- **VERIFICATION**: `_Static_assert` in `st7789_stm32_stubs.h` passed during
  the recorded build (exit 0 for the good files); the first draft with the
  wrong 0xFBFF value failed — the assert caught the author's arithmetic.
- **SOURCE**: stm32-ref-manual (I2C_SR1 bit table); cmsis (stm32f1xx header
  values match the manual).

## Quick reference table

| Fact | Value | Source table |
|---|---|---|
| ST7789 MADCTL bits | MY 0x80, MX 0x40, MV 0x20, ML 0x10, BGR 0x08, MH 0x04 | st7789-datasheet |
| MADCTL reserved | D1:D0, read-only, must be 0 (mask 0xFC) | st7789-datasheet |
| MADCTL reset | 0x00 | st7789-datasheet |
| F1 I2C_SR1 | offset 0x14, TxE=0x40 bit6, defined bits 0xDF7F | stm32-ref-manual, cmsis |
| F1 I2C block | CR1..TRISE, 9 regs, 0x24 bytes | stm32-ref-manual |
| I2C1 clock | RCC_APB1ENR bit 21 (I2C1EN) | stm32-ref-manual |
| I2C_TRISE reset | 0x0002 | stm32-ref-manual |
| F0 vs F1 I2C | F0 has ISR@0x18/ICR/TIMINGR; F1 has SR1@0x14/SR2 | stm32-ref-manual, cmsis |
