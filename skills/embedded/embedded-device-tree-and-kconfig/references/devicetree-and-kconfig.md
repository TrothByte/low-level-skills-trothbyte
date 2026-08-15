# Devicetree and Kconfig — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE(bad) →
COUNTEREXAMPLE(good) → VERIFICATION → SOURCE. Source ids refer to
registry/sources.yaml. Status tags: KNOWN = verified against the cited
spec/docs; INFERRED = from secondary sources, re-check on target.

## 1. compatible strings are keys into a binding registry, not free-form

- **RULE**: a `compatible` string must match a binding YAML path in the
  project (`dts/bindings/display/sitronix,st7789v.yaml` defines
  `compatible: "sitronix,st7789v"`). Zephyr generates the DT-derived Kconfig
  guard from it (`DT_HAS_SITRONIX_ST7789V_ENABLED`). A string the agent
  "remembers" (`"st,st7789"`, `"st7789v"` alone) binds no driver: the node
  either fails the build or silently has no driver.
- **WHY AI GETS IT WRONG**: models compress vendor prefixes and suffixes;
  `sitronix,st7789v` collapses to `st,st7789` or `st7789,st7789v`. They also
  invent `compatible` values for nodes that have no binding at all.
- **CORRECT REASONING**: for every node, state the binding file path and the
  exact `compatible` value it registers. Grep the tree for the string before
  using it. The compatible list is ordered most-specific-first; the driver
  matches by exact string, not substring.
- **EXAMPLE** (bad):
  ```dts
  st7789: st7789@0 {
      compatible = "st,st7789";        /* no binding exists */
  };
  ```
- **COUNTEREXAMPLE** (good):
  ```dts
  st7789v: st7789v@0 {
      compatible = "sitronix,st7789v"; /* dts/bindings/display/sitronix,st7789v.yaml */
  };
  ```
- **VERIFICATION**: `west build -b <board> -d build <app>` — unknown
  compatible either errors at build (no matching binding) or produces a node
  no driver probes. `dtc -I dts -O dtb` shows the raw tree but cannot catch
  missing bindings — that check is Zephyr-specific. Host stand-in
  `python examples/check_dts.py examples/bad/invented_compatible.dts` reports
  `unknown compatible 'st,st7789'`.
- **SOURCE**: devicetree-spec (§2.3.1 compatible property); zephyr-docs
  (binding YAML registry, DT_HAS_* symbol generation).

## 2. Unit-addresses must be unique among sibling nodes

- **RULE**: a node name is `node-name@unit-address`; the unit-address selects
  the entry in `reg` and must be unique among the children of one parent.
  Two children `st7789@0` and `flash@0` of `spi1` collide; the tree behavior
  for the shadowed node is undefined.
- **WHY AI GETS IT WRONG**: agents copy a template node and forget to change
  the unit-address when the chip-select/reg value changes, producing two `@0`
  children. The collision is invisible until a lookup by unit-address picks
  the wrong node.
- **CORRECT REASONING**: treat the unit-address as part of the node's identity
  within its parent. `reg = <0>` on SPI means chip-select 0 — a second device
  on CS 0 is a hardware contradiction, so `@1` + `reg = <1>` is the fix.
- **EXAMPLE** (bad):
  ```dts
  &spi1 {
      st7789@0 { compatible = "sitronix,st7789v"; reg = <0>; };
      flash@0 { compatible = "jedec,spi-nor";     reg = <0>; }; /* collision */
  };
  ```
- **COUNTEREXAMPLE** (good):
  ```dts
  &spi1 {
      st7789@0 { compatible = "sitronix,st7789v"; reg = <0>; };
      flash@1 { compatible = "jedec,spi-nor";     reg = <1>; }; /* CS 1 */
  };
  ```
- **VERIFICATION**: `dtc -I dts -O dtb` may warn; Zephyr build diagnostics
  flag ambiguous unit-addresses. Host stand-in reports
  `duplicate unit-address '0' among siblings of '&spi1'`.
- **SOURCE**: devicetree-spec (§2.2.1 node names and unit addresses).

## 3. A unit-address without reg, or reg without the right address/size cells, is a decode bug

- **RULE**: `reg` is decoded using the parent's `#address-cells` and
  `#size-cells`; a node whose name carries a unit-address but has no `reg`
  leaves the address undefined, and a unit-address that does not match the
  first reg entry breaks address lookups (`DT_REG_ADDR` in Zephyr).
- **WHY AI GETS IT WRONG**: the model edits the name (`st7789@0`) and drops
  the `reg` property, or changes the cell counts and keeps the same tuple.
- **CORRECT REASONING**: unit-address string must equal the first `reg` cell
  (as hex, lowercase, no padding requirement); keep `#address-cells`/
  `#size-cells` of the parent in mind when writing the tuple.
- **EXAMPLE** (bad):
  ```dts
  st7789v: st7789v@0 {
      compatible = "sitronix,st7789v";
      mdac = <0x68>;          /* reg missing entirely */
  };
  ```
- **COUNTEREXAMPLE** (good):
  ```dts
  st7789v: st7789v@0 {
      compatible = "sitronix,st7789v";
      reg = <0>;              /* matches @0; single #address-cell */
  };
  ```
- **VERIFICATION**: `dtc -I dts -O dtb` errors on `reg`/unit-address
  mismatch; Zephyr `DT_REG_ADDR` returns garbage for a missing reg. Host
  stand-in reports `has a unit-address but no 'reg' property`.
- **SOURCE**: devicetree-spec (§2.3.4 reg, §2.3.5 #address-cells/#size-cells).

## 4. Required binding properties are not optional

- **RULE**: Zephyr bindings mark properties `required: true`. For
  `sitronix,st7789v`: `x-offset`, `y-offset`, `mdac`, and `mipi-mode` are
  required. Dropping one makes the DT valid but the driver probe fail.
- **WHY AI GETS IT WRONG**: agents include only the properties they remember
  and treat the binding YAML as advisory.
- **CORRECT REASONING**: open the binding file and keep required properties
  in the overlay; `mipi-mode` comes from the included
  `mipi-dbi-spi-device.yaml`. Changing a required property requires touching
  both the DTS and (for runtime params like `mdac`) the driver config.
- **EXAMPLE** (bad): overlay with `compatible`, `reg`, `mdac` but no
  `x-offset`/`y-offset`/`mipi-mode`.
- **COUNTEREXAMPLE** (good): overlay carrying all required properties
  (`x-offset = <0>; y-offset = <0>; mdac = <0x68>; mipi-mode = <0>;`).
- **VERIFICATION**: `west build -b <board>` — missing required property is a
  binding error (deviicetree validator) or a probe-time `-ENODEV`. Host
  stand-in does not model required-property checks (dtc cannot either;
  Zephyr-specific).
- **SOURCE**: zephyr-docs (binding YAML, required properties);
  devicetree-spec (§2.3.1).

## 5. Kconfig driver symbols are gated by devicetree, and fabricated symbols are silent no-ops

- **RULE**: the ST7789V driver is `config ST7789V` (bool), default y,
  `depends on DT_HAS_SITRONIX_ST7789V_ENABLED`, `select MIPI_DBI`. A Kconfig
  symbol that is not defined anywhere (`CONFIG_ST7789=y`) is a silent no-op:
  Kconfig warns at best and the build proceeds with the driver off.
- **WHY AI GETS IT WRONG**: models invent plausible symbols (`CONFIG_ST7789`,
  `CONFIG_ST7789V_DRIVER`) because they remember the *shape* of Kconfig names
  but not the tree. Since unknown `CONFIG_*` in a prj.conf do not fail the
  build, the lie survives to the log.
- **CORRECT REASONING**: every `CONFIG_*` in a fragment must resolve to a
  `config` entry reachable in the build (search `drivers/display/Kconfig*`).
  Driver symbols are usually auto-selected by the DT node; the manual choice
  only overrides defaults. Verify with `west build` and check the generated
  `.config` contains the symbol with `=y`.
- **EXAMPLE** (bad):
  ```
  CONFIG_ST7789=y   # undefined symbol — build succeeds, driver disabled
  ```
- **COUNTEREXAMPLE** (good):
  ```
  CONFIG_ST7789V=y  # defined in drivers/display/Kconfig.st7789v
  ```
- **VERIFICATION**: `west build -b <board> -d build` then
  `grep ST7789 build/zephyr/.config` — only symbols that exist appear; the
  fabricated one never shows up. On this host (no west): documented, not run.
- **SOURCE**: zephyr-docs (Kconfig, DT_HAS_* symbol generation).

## Quick reference table

| Fact | Value | Source |
|---|---|---|
| ST7789V compatible | `"sitronix,st7789v"` | zephyr-docs (binding YAML, fetched 2026-08-15) |
| Driver Kconfig symbol | `ST7789V` | zephyr-docs (Kconfig.st7789v) |
| DT guard symbol | `DT_HAS_SITRONIX_ST7789V_ENABLED` | zephyr-docs |
| Required ST7789V props | x-offset, y-offset, mdac, mipi-mode | zephyr-docs |
| Node name form | `name@unit-address`, unique per parent | devicetree-spec §2.2.1 |
| reg decode | via parent #address-cells/#size-cells | devicetree-spec §2.3.4 |
| compatible form | ordered string list, exact match | devicetree-spec §2.3.1 |
| Host verification | `python examples/check_dts.py <dts>` (stand-in, not dtc) | this skill |
