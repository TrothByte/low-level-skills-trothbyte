---
name: meta-routing
description: Use at the start of any low-level task to choose the minimal relevant skill set. Prevents "load everything" behavior, enables dependency expansion, and routes to the correct skill from the registry.
---

# Meta: Skill Routing

## When to use

- Beginning any task involving C/C++/Rust/asm/FFI/embedded/kernel/binary code.
- When multiple skills could plausibly apply and you must pick the minimal set.

## What the agent often gets wrong

- "Load all relevant skills to be safe" — massive prompt, degraded performance, token waste.
- Picking a skill by name without checking its trigger description.
- Loading a deep reference that the task doesn't need yet.

## How to reason correctly

1. Map the task to skill `description` fields (that is all the selector sees — issue #216).
2. Select the smallest set whose `require` dependencies cover the task; add `recommend` only on demand.
3. Expand dependencies minimally via `registry/cross-links.yaml`, not by loading whole trees.
4. If a task needs no skill, do not load one.

## Routing rules (from registry/evals.yaml)

| Task | Expected | Forbidden |
|---|---|---|
| write C string copy | `c-string-and-buffer-safety` | rust/embedded skills |
| C→Rust FFI struct | `ffi-boundary-cross-language`, `rust-ffi-boundary` | unrelated domains |
| -O0 vs -O2 behavior diff | `compiler-ub-assumptions` | `c-string-and-buffer-safety` |
| atomic ordering between threads | `memory-ordering-reasoning` | string safety |
| ARM ISR + firmware | `embedded-volatile-and-memory-ordering` + interrupt | PTX/GPU |

## What to verify

- The selected set is minimal: no skill in the set is unused by the task.
- Dependencies present: every `require` target is loadable and registered.

## When not to use

- When the task has no low-level surface (pure business logic) — no skill loads at all.
- When a domain skill is already active for this exact task — do not add overlapping skills.

## How to verify

- Re-run the routing decision after loading: is every loaded skill exercised by the task?
- If a loaded skill was never used, remove it and note the miss.

## Where the knowledge comes from

- `registry/cross-links.yaml`, `registry/skills.yaml`, `docs/architecture.md` (routing section).

## Related skills

- `meta-evidence` — evidence rules apply to routing claims ("this skill covers X").
- `meta-completion` — routing is part of a clean task start.

## Evaluation

- Routing evals in `registry/evals.yaml` (routing.cases): task → expected/optional/forbidden
  skills. Score on minimal set (no unused skills) and correct dependencies.
