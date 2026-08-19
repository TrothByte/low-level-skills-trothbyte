---
name: meta-routing
description: Use at the start of any low-level task to choose the minimal relevant skill set. Prevents "load everything" behavior, enables dependency expansion, and routes to the correct skill from the registry.
---

# Meta: Skill Routing

## When to use

- Beginning any task involving C/C++/Rust/asm/FFI/embedded/kernel/binary code.
- When multiple skills could plausibly apply and you must pick the minimal set.

## When not to use

- When the task has no low-level surface (pure business logic) — no skill loads at all.
- When a domain skill is already active for this exact task — do not add overlapping skills.
- When the task is high-level application work (web, UI, database, Python/JavaScript, business logic) — do not load low-level skills unless the task explicitly requires low-level work.

## Routing limits (v3.0)

1. **HARD LIMIT: maximum 3 skills per task.** If routing suggests more, pick the top 3 by relevance, then re-check minimality: every loaded skill must be exercised by the task.
2. **NEGATIVE TRIGGERS.** If the task mentions python, javascript/js, web, business logic, high-level application code, or UI/DB — do NOT load low-level skills (C/Rust/asm/embedded/kernel) unless the task explicitly requires low-level work.
3. **CONFIDENCE GATE.** If routing confidence < 0.7, ask the user for clarification instead of guessing.

## What the agent often gets wrong

- "Load all relevant skills to be safe" — massive prompt, degraded performance, token waste.
- Picking a skill by name without checking its trigger description.
- Loading a deep reference that the task doesn't need yet.
- Loading more than 3 skills "to cover all bases" — a bigger set degrades reasoning.

## How to reason correctly

1. Map the task to skill `description` fields (that is all the selector sees — issue #216).
2. Select the smallest set whose `require` dependencies cover the task; add `recommend` only on demand.
3. Expand dependencies minimally via `registry/cross-links.yaml`, not by loading whole trees.
4. If a task needs no skill, do not load one.

### Routing rules (from registry/evals.yaml)

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
- Set size ≤ 3: no skill beyond the third was justified by the task.

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

## CRITICAL REMINDER

Two absolute prohibitions for this skill:

- NEVER load more than 3 skills per task — a bigger set degrades reasoning and wastes tokens; keep the set minimal even when many skills plausibly match.
- NEVER guess a routing when confidence < 0.7 — ask the user for clarification; a wrong guess costs more than a question.
