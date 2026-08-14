# dwarf — Skills

DWARF is the debug information format. This skill teaches reading debug info and — critically — why optimized builds show 'value optimized out' and how to debug them anyway.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `dwarf-debug-info` | Use when reading or generating DWARF debug info, mapping addresses to source lines, explaining "value optimized out" in optimized builds, writing debug-friendly code, or inspecting binaries with objdump/readelf/gdb. Teaches DWARF sections, DIEs, attributes, location lists, and optimized-code debugging strategies. | source-backed | `skills/dwarf/dwarf-debug-info` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
