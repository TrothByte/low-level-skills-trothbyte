# Evaluation — embedded-board-bringup-peripheral-init

Skill: `skills/embedded/embedded-board-bringup-peripheral-init`.
Stability target: `evaluated`.

## Verified facts (host, recorded 2026-08-15)

The host-runnable core (gray-code encoder state machine + init-order checker)
was actually compiled and executed:

```
gcc -Wall -Wextra -Werror -O2 examples/good/gray_code_encoder.c -o enc
  run:
  "gray-code encoder position after full cycle: 0"
  "PASS: position tracked exactly"          (exit 0)

gcc -Wall -Wextra -Werror -O2 examples/good/init_order_check.c -o ioc
  run:
  "bad_order (config before clock): rejected (correct)"
  "good_order (clock before config): accepted (correct)"   (exit 0)

gcc -Wall -Wextra -Werror -O2 examples/bad/encoder_edge_count.c -o encbad
  run:
  "edge-count position after 8 transitions: 2"
  "(expected +2 for 8 gray steps; edge counting undercounts)"   (exit 0)
```

The bad edge-count driver is the mcuoneclipse 2025 failure shape: 8
transitions of the same 2-bit sequence yield position 2 (two rising edges of
channel A), while the gray-code state machine yields the full count (7
transitions, -7 on reverse). It compiles clean — the bug is silent, must be
caught by review.

The register-level target code (`clock_order.c` good/bad, `gpio_af_init.c`,
`invented_register.c`) requires STM32 headers and an ARM toolchain; it was
REVIEWED against the datasheet rules, NOT compiled here (no
`arm-none-eabi-gcc`, no CMSIS headers on this host). Honest status: the
state-machine and ordering logic is host-verified; the register targets are
documented for the ARM/QEMU toolchain.

## Synthetic evals

- easy/negative: `bad/encoder_edge_count.c` — single-channel edge counting.
- easy/negative: `bad/clock_order.c` — config before clock enable.
- medium/negative: `bad/invented_register.c` — register bit not in datasheet.
- easy/positive: `good/gray_code_encoder.c` — 2-bit gray-code state machine.
- medium/positive: `good/clock_order.c` — clock-tree-first init.
- medium/positive: `good/gpio_af_init.c` — AF mode + correct AF number.
- easy/positive: `good/init_order_check.c` — host-run ordering checker.

## False-positive evals (correct code must not be flagged)

- AF mode + pull-up config on an input pin (encoder/button) — correct.
- PLL configuration in the documented order (source → PLL → bus prescalers →
  peripheral enable) — do NOT flag.
- A `volatile uint32_t *` register access — correct, do NOT flag.
- A timer configured AFTER its clock enable with matching read-back — approve.

## Historical evals (mcuoneclipse 2025)

- Class: quadrature encoder init "looks correct but wrong" — position drifts
  because the driver counts edges of one channel and guesses direction from the
  other's level. The agent must require the 2-bit gray-code state machine and
  verify against the full transition table (host run above).
- Verify: feed the forward+reverse gray sequence; assert exact tracking
  (recorded above).

## Adversarial evals

- A CMSIS-named register that does not exist for the exact part (compiles via
  family-superset headers) — must be caught by datasheet cross-check.
- An init sequence that enables the peripheral clock after configuring its
  registers and "works" on a debugger because the debugger idles differently —
  must be flagged as ordering-dependent.
- A UART init with correct USART registers but the pin in GPIO output mode —
  must be flagged as missing AF configuration.

## Verification commands (target — documented, NOT run here)

```
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -Wall -Wextra -Werror \
  -c examples/good/gpio_af_init.c
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -Wall -Wextra -Werror \
  -c examples/good/clock_order.c
qemu-system-arm -machine netduino2 -kernel app.elf      # STM32F405 model
# scope/loopback: verify TX pin produces the UART waveform; encoder wheel test
```

## Scoring

- precision: every flagged file maps to a named reference rule.
- recall: all bad files detected (edge counting, clock order, invented regs).
- FP-rate: correct init patterns produce zero flags.
