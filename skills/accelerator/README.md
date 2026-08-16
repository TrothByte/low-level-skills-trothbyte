# accelerator — Skills

AI accelerator pipeline programs: cross-unit (DMA/vector/matrix/scalar) synchronization and barrier coverage on shared on-chip buffers.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `accelerator-pipeline-synchronization` | Use when writing or reviewing AI-accelerator pipeline programs (DMA, vector, matrix, scalar units on shared on-chip buffers): checking barrier/sync coverage across units, because missing or misplaced synchronization escapes simulation and golden testing. Teaches happens-before reasoning over cross-unit write-read pairs. | unique | researched | `skills/accelerator/accelerator-pipeline-synchronization` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
  (`references/` hold deep knowledge; `examples/good` and `examples/bad` are compiled/run
  fixtures; `evals/README.md` defines eval cases.)
- Load only the skill you need (see `skills/_meta/meta-routing`; references load on demand.

## Related

[Back to repository root](../../README.md)
