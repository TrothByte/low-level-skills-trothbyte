# Board Bring-Up and Peripheral Init — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.

## 1. Clock tree before peripheral registers

- **RULE**: initialization order is: clock source (HSI/HSE/PLL) → bus clock
  enable (AHB, APB1/APB2) → peripheral register configuration. A peripheral
  whose bus clock is not enabled ignores every register write.
- **WHY AI GETS IT WRONG**: agents emit the "obvious" register writes for the
  peripheral and forget the clock-enable step or place it after the config.
  Result: "the code looks right, the peripheral does nothing."
- **CORRECT REASONING**: on STM32-class parts the peripheral's APB/AHB clock
  gate must be set before the peripheral exists on the bus; writes before that
  are dropped (and often hang on a disabled bus). Verify the enable bit name
  (`RCC->APB1ENR`, `RCC->AHB1ENR`, etc.) in the datasheet's RCC section for the
  exact part.
- **EXAMPLE** (bad):
  ```c
  // configure the timer without enabling its clock
  TIM2->PSC = 71; TIM2->ARR = 999; TIM2->CR1 |= 1;   // does nothing
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;   // clock first
  TIM2->PSC = 71; TIM2->ARR = 999; TIM2->CR1 |= 1;
  ```
- **VERIFICATION**: toggle the enable and observe the peripheral registers
  become writable (read-back before/after) in a debugger.
- **SOURCE**: stm32-ref-manual (RCC chapter, clock tree); cmsis (bit names).

## 2. GPIO alternate function is mandatory for UART/TIM/SPI/I2C

- **RULE**: pins used by a peripheral must be in GPIO alternate-function mode
  with the correct AF number for the exact pin, not in generic output mode. A
  UART pin in output mode produces silence regardless of UART register setup.
- **WHY AI GETS IT WRONG**: the model sets the pin as digital output "because
  it should drive something" or skips the AF number; the peripheral and the
  pin never connect.
- **CORRECT REASONING**: on STM32, the AFR register maps each pin to AF0–AF15;
  the datasheet's alternate-function table lists which AF number connects the
  pin to which peripheral. Setting MODER=AF is necessary but not sufficient —
  the AF number must match the peripheral instance and pin.
- **EXAMPLE** (bad):
  ```c
  GPIOA->MODER |= (3 << (9 * 2));  // PA9 = output mode; no AF — UART dead
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  // USART1 TX on PA9 = AF7; MODER=AF(10), AFRL/AFRH selects AF7
  GPIOA->MODER = (GPIOA->MODER & ~(3u << 18)) | (2u << 18); // AF mode
  GPIOA->AFRH  = (GPIOA->AFRH & ~(0xF << 4)) | (7u << 4);   // AF7
  ```
- **VERIFICATION**: read back MODER/AFR and check the actual AF value; scope the
  pin or run a loopback UART test.
- **SOURCE**: stm32-ref-manual (GPIO chapter, AF tables); cmsis.

## 3. Quadrature encoders are 2-bit gray-code state machines

- **RULE**: a quadrature (A/B) encoder increments/decrements per state
  transition of the two-bit (A,B) signal, following the gray-code cycle
  00→01→11→10 (or reverse). Direction = the transition order. Counting edges
  of a single channel misreads direction and loses counts.
- **WHY AI GETS IT WRONG**: the mcuoneclipse 2025 class — init code "looks
  correct" but position drifts because the driver counts rising edges of A and
  assumes direction from the level of B; this breaks at high speed, at rest,
  and during reverse.
- **CORRECT REASONING**: maintain the previous (A,B) pair; on each change,
  decode the next state relative to the gray-code cycle. +1 for one cycle
  direction, -1 for the other. Handle invalid skips (missed transitions) by
  bounded correction rather than trusting a single edge.
- **EXAMPLE** (bad):
  ```c
  if (A_rising_edge()) pos += (B ? 1 : -1);   // edge counting: loses counts,
                                              // wrong near rest
  ```
- **COUNTEREXAMPLE** (good): a `decode_gray(a, b)` function mapping
  (prev,cur) state pairs to +1/-1/0, driven on every change of A or B.
- **VERIFICATION**: feed the state machine the full gray-code sequence forward
  and backward; assert position increments/decrements exactly (host-runnable —
  see examples).
- **SOURCE**: mcuoneclipse 2025 (empirical); stm32-ref-manual (TIM encoder
  mode exists on many parts — use it when available).

## 4. Never invent registers: datasheet per exact part number

- **RULE**: every register name and bit used must exist in the reference manual
  for the EXACT part (STM32F103 vs F407 have different clocks/AF). A name from
  CMSIS headers compiling proves only that some part has it.
- **WHY AI GETS IT WRONG**: agents produce code that "compiles" against CMSIS
  but targets registers that don't exist for the part, sometimes switching MCU
  assumptions mid-session.
- **CORRECT REASONING**: before generating a write, locate it in the manual:
  section, offset, reset value, bit meaning. Cross-check the part's family
  superset and the specific part's presence of the peripheral (e.g. which TIM
  has encoder mode). If unsure, prefer the vendor HAL function (which encodes
  the datasheet).
- **EXAMPLE** (bad): `RCC->CFGR |= RCC_CFGR_USBPRE` on a part without that bit
  (compiles via headers, silently ignored).
- **COUNTEREXAMPLE** (good): verify `USBPRE` in the part's RCC_CFGR table
  before using it, or use `HAL_RCC_...` which abstracts it.
- **VERIFICATION**: grep the reference manual for the register + bit; confirm
  the reset value and that the peripheral exists on the part.
- **SOURCE**: stm32-ref-manual; cmsis (headers describe a family superset).

## 5. Peripheral enable before use, and volatile access

- **RULE**: enable a peripheral's clock/control gate before configuring its
  registers, and access hardware registers only through volatile-qualified
  pointers (CMSIS does this for you; hand-rolled code must too).
- **WHY AI GETS IT WRONG**: agents rely on the compiler keeping writes in order
  and on registers being ordinary memory; reordering or caching can drop the
  init entirely.
- **CORRECT REASONING**: hardware registers change outside the C abstract
  machine; the compiler must not eliminate or reorder them. Use the CMSIS
  volatile pointers, or declare `volatile` in your own struct. Sequencing the
  enable before the config writes is part of the contract (rule 1).
- **EXAMPLE** (bad): `uint32_t *ctrl = (uint32_t *)0x40021000; *ctrl |= 1;`
  — no volatile.
- **COUNTEREXAMPLE** (good): `volatile uint32_t *ctrl = ...;` or use CMSIS
  `TIM2->CR1` which is already volatile.
- **VERIFICATION**: inspect the disassembly — writes must appear in order and
  not be eliminated at -O2.
- **SOURCE**: cmsis (volatile register definitions); stm32-ref-manual.

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Clock tree | power → clock source → bus clock enable → peripheral config |
| GPIO AF | UART/TIM/SPI pins need AF mode + the correct AF number |
| Encoders | 2-bit gray-code state machine, not single-channel edge counting |
| Datasheet | every register verified against the EXACT part's manual |
| Volatile | registers accessed through volatile pointers, ordered writes |
