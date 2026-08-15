---
name: embedded-device-tree-and-kconfig
description: Use when writing or reviewing Zephyr/DeviceTree overlays and Kconfig fragments — compatible strings, unit-addresses, reg, required binding properties, and driver Kconfig symbols. Prevents invented compatible strings and Kconfig symbols that build silently but leave hardware undriven.
---

# Devicetree and Kconfig

## When to use

- Writing or reviewing Zephyr devicetree overlays (`.dts`/`.dtsi`) and board
  defconfigs (`prj.conf`, `Kconfig` fragments).
- Adding an external device (display, sensor, flash, GPIO expander) to an
  existing board.
- Debugging "builds fine, hardware never initializes" where the cause is a
  wrong `compatible`, a node nobody binds to, or a `CONFIG_*` that does not
  exist.
- Deciding which properties are required for a given binding.

## When not to use

- Bare-metal register-level drivers — use `embedded-hw-register-datasheet-verification`.
- Bootloader/rootfs or non-Zephyr embedded builds without devicetree.
- Making up a device tree from scratch when no binding exists — that needs a
  binding YAML first (covered here as a rule, but creating bindings is a
  different task).

## What the agent often gets wrong

- Fabricated `compatible` strings (`"st,st7789"` instead of
  `"sitronix,st7789v"`) that bind to no driver.
- Duplicate unit-addresses (`st7789@0` and `flash@0` under the same `&spi1`)
  that shadow one node.
- Unit-addresses with no `reg` property, or `reg` tuples that ignore
  `#address-cells`/`#size-cells`.
- Dropping binding-required properties (`x-offset`, `y-offset`, `mdac`,
  `mipi-mode` for ST7789V), leaving the probe to fail.
- Invented Kconfig symbols (`CONFIG_ST7789=y`) that are silent no-ops because
  Kconfig does not fail on undefined `CONFIG_*` in fragments.
- Confusing `CONFIG_ST7789V` (the driver) with the DT guard
  `DT_HAS_SITRONIX_ST7789V_ENABLED` that gates it.

## How to reason correctly

1. For every node, name the binding file that defines it and quote its exact
   `compatible` value; grep the tree before trusting a string.
2. Check unit-address uniqueness among siblings and that the unit-address
   equals the first `reg` entry decoded by the parent's address/size cells.
3. List the binding's `required: true` properties and include all of them.
4. Treat every `CONFIG_*` as a claim: find the `config` statement that defines
   it, and remember DT-dependent symbols exist only when the node is enabled.
5. Remember that Zephyr DT checks (bindings, required props) run during
   `west build`, while pure DT syntax is checked by `dtc`. Both are needed.

## What to verify

- Compatible strings exist in `dts/bindings/**/*.yaml` with the same string.
- No two siblings share a unit-address; each unit-address has `reg`.
- All required properties of the binding are present with sane values.
- Kconfig symbols resolve to real `config` entries.
- The generated `.config` contains the driver symbol as `=y`.

## How to verify

```
dtc -I dts -O dtb -o out.dtb board_overlay.dts      # pure syntax/structure
west build -b <board> -d build -p always <app>      # bindings + Kconfig gate
grep -E "ST7789V|MIPI_DBI" build/zephyr/.config     # did the symbol resolve?
python examples/check_dts.py examples/good/display_overlay.dts
python examples/check_dts.py examples/bad/*.dts     # host stand-in, reports:
#   unknown compatible 'st,st7789'
#   duplicate unit-address '0' among siblings of '&spi1'
#   node 'st7789v@0' has a unit-address but no 'reg' property
```

`check_dts.py` is a host-side stand-in for the three structural rules on the
example overlays; it is NOT a substitute for `dtc` or `west`. On this host
`dtc` and `west` are not installed — those two commands are documented and
researched, not run.

## Where the knowledge comes from

- `devicetree-spec` — node names and unit addresses (§2.2.1), compatible
  (§2.3.1), reg and address/size cells (§2.3.4–2.3.5).
- `zephyr-docs` — binding YAML registry and DT_HAS_* Kconfig symbol
  generation; ST7789V binding and `Kconfig.st7789v` re-fetched 2026-08-15
  (compatible `"sitronix,st7789v"`, symbol `ST7789V`,
  guard `DT_HAS_SITRONIX_ST7789V_ENABLED`, required props x-offset/y-offset/
  mdac/mipi-mode — KNOWN).

## Related skills

- `embedded-hw-register-datasheet-verification` — same verify-vs-source
  discipline for raw registers; DT nodes ultimately describe those registers.
- `qemu-system-setup` — running the built Zephyr image for runtime probing.
- `rtos-concurrency-and-isr`, `embedded-interrupt-and-nested` — downstream of
  a working device tree.

## Evaluation

- Synthetic: bad overlays must be rejected (invented compatible, duplicate
  unit-address, missing reg); the good overlay must pass the host checker.
- False-positive: a correct overlay with two devices on different chip
  selects (`@0` and `@1`) must pass; `CONFIG_ST7789V=y` with a real DT node
  must not be flagged as invented.
- Adversarial: a subtle misspelling of a valid compatible (`sitronix,st7789`)
  and a duplicate unit-address hidden at different indentation depths must be
  caught.
- Historical: no curated corpus of real DT hallucination bugs is registered —
  the "fabricated Kconfig symbol" case is reproduced as a fixture instead
  (UNVERIFIED against upstream history).
- Researched gap: `dtc`/`west` absent on this host; recorded commands are
  exact but not executed. The host stand-in's actual output is in
  `evals/README.md`.
