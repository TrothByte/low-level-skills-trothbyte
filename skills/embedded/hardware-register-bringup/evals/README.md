# Evaluation — hardware-register-bringup

Skill: `skills/embedded/hardware-register-bringup`. Stability target:
`evaluated`. Register/sequence knowledge KNOWN from stm32-ref-manual /
cmsis / devicetree-spec / openocd-docs. Init-order and sequence fixtures
EXECUTED on this host (gcc 16.1.0, python 3.11.9). Target toolchain
(arm-none-eabi-gcc, QEMU, real silicon) UNVERIFIED.

## Synthetic evals

| Case | Fixture | Expected | Status |
|---|---|---|---|
| easy/negative | `bad/init_sequence_bad.c` | config-before-clock + no ready-poll → flagged | executable |
| easy/negative | `bad/sequence_missing_reset.py` | reset never deasserted → flagged | executable |
| medium/negative | `bad/guessed_register.c` | guessed clock bit, no datasheet check → flagged | executable |
| easy/positive | `good/init_sequence_good.c` | clock→ready→reset→config→enable order correct | executable |
| easy/positive | `good/sequence_check.py` | order analyzer passes good, catches bad | executable |

Detection rule: (1) every register field traceable to the datasheet;
(2) sequence order power→reset→clock-ready→bus clocks→peripheral
clock→reset-deassert→config→enable; (3) ready flags polled with
timeout; (4) RMW touches only documented fields.

## False-positive evals (correct code must NOT be flagged)

- A peripheral with no reset register (always out of reset) — no
  deassert step is correct.
- Polling a ready flag with a bounded timeout loop — correct, not
  "wasted cycles".
- Writing the full datasheet reset value instead of RMW — correct.
- A vendor HAL doing the clock/reset ordering internally — the agent
  should not re-derive it.

## Historical evals

- Config-before-clock and stuck-in-reset bugs are documented embedded
  failure classes (mcuoneclipse/EE community write-ups, ST community
  forum threads). UNVERIFIED as named incidents on this host.
- The guessed-register-bit class ("compiles via CMSIS, wrong on silicon")
  is a documented AI-code-generation failure pattern in embedded
  benchmarks. UNVERIFIED as a named incident here.

## Adversarial evals

- Code that "works" in a simulator because the simulator ignores clock/
  reset gating, but does nothing on silicon — the
  `meta-verification-harness-validity` trap.
- A guessed register bit that compiles clean via CMSIS headers but does
  not exist for the part — only the datasheet check catches it.
- A ready-flag poll with an infinite loop (hangs on silicon if the flag
  never sets) vs a bounded timeout.
- An RMW that ORs a reserved bit by mistake, enabled only at runtime on
  real hardware.

## Verification commands

```
gcc -Wall -Wextra -Werror -O2 examples/good/init_sequence_good.c -o seqgood && seqgood
gcc -Wall -Wextra -Werror -O2 examples/bad/init_sequence_bad.c -o seqbad && seqbad
python examples/good/sequence_check.py
python examples/bad/sequence_missing_reset.py
gcc -Wall -Wextra -Werror -O2 -c examples/good/rcc_init.c
gcc -Wall -Wextra -Werror -O2 examples/bad/guessed_register.c -o guessbad && guessbad

# Target (documented; not installed on this host):
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -c examples/good/rcc_init.c
qemu-system-arm -machine netduino2    # STM32F405 model
```

## Verified facts

- KNOWN: clock-before-config; reset-deassert after clock and before
  config; ready-flag polling with timeout; datasheet-first register
  fields; RMW on documented fields; enable-bit-last. Sources:
  stm32-ref-manual, cmsis, devicetree-spec, openocd-docs, esp-idf-docs
  (INFERRED).
- EXECUTED on this host: `seqgood` PASS; `seqcheck.py` PASS (catches the
  bad sequence); `seqbad` and `sequence_missing_reset.py` demonstrate
  the bugs (recorded below); `rcc_init.c` compiles with `_Static_assert`.
- UNVERIFIED: arm-none-eabi-gcc build, QEMU board model, real-silicon
  bring-up on this host.

## Scoring

- precision: every flagged issue maps to a reference rule (1–6).
- recall: config-before-clock, missing-reset-deassert, guessed-register,
  and no-ready-poll classes detected.
- FP-rate: no-reset-register peripherals and bounded ready-poll loops
  produce zero flags.
- Decisive test: "is every register write preceded by clock enable and
  reset deassert, with ready flags polled and fields datasheet-verified?"

### Executed output (2026-08-17, MSYS2 gcc 16.1.0 / python 3.11.9)

```
$ gcc -Wall -Wextra -Werror -O2 examples/good/init_sequence_good.c -o seqgood && ./seqgood
PASS: clock -> ready-poll -> reset-deassert -> config -> enable
exit 0

$ gcc -Wall -Wextra -Werror -O2 examples/bad/init_sequence_bad.c -o seqbad && ./seqbad
WARN: writing config with clock disabled (ignored on silicon)
BUG: proceeding without polling PLL ready flag
init done
exit 0   (flagged: both init-order violations)

$ python examples/good/sequence_check.py
PASS: init-order analyzer catches clock/reset/enable violations
exit 0

$ python examples/bad/sequence_missing_reset.py
BUG: config while reset still held (silently ignored on silicon)
exit 1

$ gcc -Wall -Wextra -Werror -O2 examples/good/rcc_init.c -c
(no output; compile-time _Static_assert passes)
exit 0

$ gcc -Wall -Wextra -Werror -O2 examples/bad/guessed_register.c -o guessbad && ./guessbad
BUG: guessed clock-enable bit (no datasheet check)
exit 0   (flagged by review)
```
