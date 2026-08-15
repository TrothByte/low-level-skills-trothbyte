---
name: embedded-board-bringup-peripheral-init
description: Use when bringing up a new embedded board or peripheral: clock-tree and init-order reasoning, GPIO alternate-function configuration, register guessing for unknown MCUs, and quadrature-encoder interpretation. Prevents "looks correct but wrong" init code and naive level-counting encoders. Requires datasheet-first reasoning and state-machine logic for gray-code signals.
---

# Embedded Board Bring-Up and Peripheral Initialization

## When to use

- Bringing up a new board/MCU: first blink, UART, GPIO, timers, quadrature
  encoder, ADC/DAC, SPI/I2C.
- Diagnosing a peripheral that "looks correct but doesn't work" — wrong clock,
  wrong pin mux, wrong init order.
- Interpreting datasheet register tables for an MCU you have not memorized.
- Writing a quadrature-encoder driver (position/velocity from A/B signals).
- Reviewing generated (LLM) init code against the datasheet before flashing.

## When not to use

- Peripheral logic that is already wired and working (application code) — not
  bring-up.
- CPU core/architecture details (ARM core registers) — different skill set.
- Production drivers for well-known chips with full HAL support where you
  should use the vendor HAL (CMSIS/ESP-IDF/Zephyr) instead of hand-rolled
  registers.
- Debugging already-running code with a debugger — use
  `embedded-flash-debug-cycle`.

## What the agent often gets wrong

- Guesses register names/offsets for an obscure MCU "even within one session":
  `RCC->CFGR |= GPIOA_CLK_EN` instead of checking the actual clock-enable bit
  in the datasheet's RCC section — compiles via CMSIS headers, silently wrong.
- Skips the clock tree: enables the peripheral's clock but not its bus or PLL,
  or enables a peripheral clock AFTER configuring its registers. Wrong clock →
  "peripheral does nothing".
- Sets the GPIO mode to digital output but forgets the alternate-function
  selection for the peripheral pin (UART/TIM/SPI need AF mode, not output);
  or sets output but no pull-up/pull-down where the encoder/switch needs it.
- Implements a quadrature encoder by counting edges of one channel ("A went
  high → +1") — this misreads direction and misses counts at high speed;
  mcuoneclipse 2025 documented the "looks correct but wrong" encoder class.
- Reads one register table, writes init code, and never re-checks the
  datasheet for the *specific* MCU part number (STM32F103 vs STM32F407 clock
  and AF tables differ).

## How to reason correctly

1. Datasheet-first: for every register you touch, verify the field name, offset,
   and reset value in the MCU's reference manual for the EXACT part number.
   CMSIS headers give you names, not guarantees that a name exists for your
   part.
2. Clock tree before peripheral: power → clock source (HSI/HSE/PLL) → bus clock
   enable (AHB→APB1/APB2) → peripheral enable. Only then configure the
   peripheral registers. Wrong order = device "dead" despite correct-looking
   register writes.
3. Pin mux last but mandatory: GPIO mode (input/output/AF/analog), then the
   alternate function number for the peripheral (AF0–AF15), plus
   pull-up/pull-down and speed. A UART on the right pin in the wrong AF mode
   produces silence.
4. Encoder: treat A/B as a 2-bit state machine over gray-code states
   (00,01,11,10). One full transition per count step; direction from
   state-transition order. Never count a single channel's edges.
5. When the model is unsure, ask the datasheet: search the reference manual for
   the exact register name and the exact MCU variant before generating code.

## What to verify

- Every register written exists in the datasheet for the exact part; no
  invented fields.
- Clock enable order: peripheral clock enabled BEFORE peripheral register
  config; bus/PLL clocks configured and stable.
- GPIO mode matches the peripheral (AF for UART/TIM/SPI/I2C; the correct AF
  number; input+pull for encoders/buttons).
- Quadrature driver is a 2-bit state machine (gray code), not single-channel
  edge counting.
- Compiled target code has no warnings and the object disassembles to the
  intended register offsets (for host-compilable logic, run the state machine
  directly).

## How to verify

```
# host-verifiable core: the encoder state machine and AF/clock logic
gcc -Wall -Wextra -Werror -O2 examples/good/gray_code_encoder.c -o enc
enc                        # runs the 4-state machine, checks direction + counts

# target (documented; not on this host — no arm toolchain):
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -c examples/good/gpio_af_init.c
# or QEMU: qemu-system-arm -machine netduino2  (models an STM32F405)
```

The state-machine logic is host-verifiable; the register code needs the
datasheet (sources below) and the target toolchain — recorded status in
`evals/README.md`.

## Where the knowledge comes from

- `stm32-ref-manual` — RCC/GPIO/EXTI/TIM register maps, AF tables, clock tree.
- `cmsis` — the header names and types used by init code (and their
  part-specific limits).
- `mcuoneclipse` 2025 — quadrature encoder "looks correct but wrong" incident
  analysis. (Referenced empirically.)

## Related skills

- `embedded-flash-debug-cycle` — getting this code onto the board.
- `embedded-hil-ci-testing` — verifying init on real hardware.
- `embedded-ota-bootloader-safety` — safe field updates of firmware.
- `embedded-volatile-and-memory-ordering` — why register writes must be
  volatile and ordered.

## Evaluation

- Synthetic: flag bad/encoder_edge_count.c (single-channel counting), flag
  bad/clock_order.c (peripheral configured before its clock); approve
  good/gray_code_encoder.c and the AF/clock-order pattern.
- False-positive: correct AF output + pull-up config, correct PLL
  configuration order must NOT be flagged.
- Historical/adversarial: the mcuoneclipse 2025 encoder bug class; a register
  name that compiles via CMSIS but does not exist for the part must be caught.
- Verified facts and commands: `evals/README.md`.
