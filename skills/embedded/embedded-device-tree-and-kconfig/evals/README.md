# Evaluation — embedded-device-tree-and-kconfig

Skill: `skills/embedded/embedded-device-tree-and-kconfig`.
Toolchain status: RESEARCHED. `dtc`, `west`, and a Zephyr SDK are NOT installed
on this host. Exact verification commands are recorded and were not run;
a host-side python stand-in (`examples/check_dts.py`) implements the three
structural rules on the fixture overlays and its output is recorded below.

## Synthetic evals (host stand-in, run 2026-08-15)

Command: `python examples/check_dts.py good/display_overlay.dts bad/invented_compatible.dts bad/address_collision.dts bad/missing_reg.dts`

Recorded output (exit 1 overall):

```
PASS .../good/display_overlay.dts
FAIL .../bad/invented_compatible.dts:8: unknown compatible 'st,st7789'
    (not in binding registry)
FAIL .../bad/address_collision.dts:13: duplicate unit-address '0'
    among siblings of '&spi1'
FAIL .../bad/missing_reg.dts:7: node 'st7789v@0' has a unit-address
    but no 'reg' property
```

## Researched evals (toolchain absent — exact commands, NOT run)

| Case | Fixture | Expected | Command |
|---|---|---|---|
| fabricated compatible | `bad/invented_compatible.dts` | binding error or undriven node | `west build -b <board> -d build -p always <app>` |
| duplicate unit-address | `bad/address_collision.dts` | dtc warning / build diagnostic | `dtc -I dts -O dtb -o out.dtb <overlay>.dts` |
| missing reg | `bad/missing_reg.dts` | dtc error on unit-address/reg mismatch | `dtc -I dts -O dtb -o out.dtb <overlay>.dts` |
| invented Kconfig symbol | `bad/invented_kconfig.conf` | silent no-op; `.config` lacks symbol | `west build ... && grep ST7789 build/zephyr/.config` |
| correct enable | `good/prj.conf` | `CONFIG_ST7789V=y` resolves | `grep ST7789V build/zephyr/.config` |

Honest status: `dtc` and `west` are not available in this environment; every
cell above is documented research, not executed output.

## Verified facts (ACTUAL, this host)

- Upstream Zephyr binding `dts/bindings/display/sitronix,st7789v.yaml`
  (fetched 2026-08-15): `compatible: "sitronix,st7789v"`; required properties
  `x-offset`, `y-offset`, `mdac`, `mipi-mode`; includes `mipi-dbi-spi-device.yaml`
  and `lcd-controller.yaml`.
- Upstream `drivers/display/Kconfig.st7789v` (fetched 2026-08-15): `config
  ST7789V` (bool, default y, `depends on DT_HAS_SITRONIX_ST7789V_ENABLED`,
  `select MIPI_DBI`).
- Host stand-in checker: good overlay passes, all three bad overlays fail
  with the recorded messages above (real python 3.11.9 run).

## False-positive evals

- `good/display_overlay.dts` with two devices on CS 0 and CS 1 (`@0`/`@1`,
  `reg = <0>`/`<1>`) must pass — distinct unit-addresses are not a collision.
- `CONFIG_ST7789V=y` must not be flagged when a valid
  `"sitronix,st7789v"` node exists in the tree.
- A node without a unit-address in its name (e.g. a `&spi1` override label)
  must not trigger the missing-`reg` check.

## Adversarial evals (researched)

- A valid-compatible misspelling (`"sitronix,st7789"`) is caught by the
  registry check (host stand-in covers it).
- A duplicate unit-address with different node names at different depths:
  covered by the sibling-scoped check; deeper nesting requires the stand-in
  to be extended (currently flat examples only) — documented limitation.
- A fabricated Kconfig symbol inside a `#if` block in `Kconfig*` source (not
  a fragment) — Kconfig may treat it as a definition; this host has no
  kconfiglib/Zephyr to test (UNVERIFIED).

## Historical evals

- No curated historical bug corpus for DT/Kconfig hallucinations is
  registered in this repository. Candidate: the known "invented Kconfig
  symbol survives to the build log" class of agent failures. Status:
  UNVERIFIED.

## Target toolchains (absent, documented)

- `dtc` — not installed (devicetree spec conformance check).
- `west` + Zephyr SDK — not installed (binding/Kconfig gate).
- A Linux or WSL host with `west` is the planned elevation path to make this
  skill source-backed.
