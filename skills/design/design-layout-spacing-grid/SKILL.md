---
name: design-layout-spacing-grid
description: Use when building or reviewing page layout, spacing, and responsive grids. Teaches a 4/8pt spacing scale, 12-column grids with consistent gutters, WCAG 1.4.10 reflow at 320px, and eliminating arbitrary px margins.
---

# Design Layout, Spacing & Grid

## When to use

- Building or reviewing page layout: margins, paddings, gaps, and grid systems.
- Setting up a 12-column grid or auditing an existing one for consistent gutters.
- Making a layout responsive and checking WCAG 1.4.10 Reflow at 320px.
- Reviewing generated UI with arbitrary `px` spacing (13px, 7px, 23px margins).
- Migrating hard-coded spacing to a 4/8pt scale (usually via design tokens).

## When not to use

- Tokenizing the spacing values into a design system — that is
  `design-token-system-discipline` (this skill is the layout-level consumer).
- Choosing type sizes or line-height — `design-typography-hierarchy`.
- Contrast/color accessibility — `design-color-contrast-wcag-a11y`.
- Judging whether a layout "feels original" — `design-anti-ai-look-originality-review`.

## What the agent often gets wrong

- Emits arbitrary spacing values (13px, 7px, 23px) that "fit" a mock but
  belong to no scale — the classic arbitrary-px layout smell.
- Builds a grid with `repeat(5, 1fr)` or inconsistent gutters
  (`column-gap: 12px; row-gap: 24px`) and calls it "the grid".
- Ships fixed pixel widths (`min-width: 1000px`) with no breakpoint, so the
  page scrolls two-dimensionally at 320px — a WCAG 1.4.10 Reflow failure.
- "Fixes" overflow with `overflow-x: hidden` on body instead of reflowing
  the layout (masks the failure from the checker).
- Duplicates spacing inline instead of consuming scale tokens — layout loss
  when the design system changes.

## How to reason correctly

1. Start from a scale: 4/8pt (multiples of 4; base rhythm 8). Every margin,
   padding, and gap must land on the scale — if the mock says 13px, the
   mock (or the scale) is wrong.
2. Use a 12-column grid with one gutter value (column-gap = row-gap); inner
   sections use factor columns (2/3/4/6 = 12-based splits), never 5 or 7.
3. Design for 320px first: fluid `fr`/`%`/`clamp()` units, breakpoints via
   `@media`, never fixed widths above 320px, never `overflow-x: hidden` as
   the "solution".
4. Feed spacing through tokens so one change re-themes the whole layout.
5. Verify with the two checkers: spacing/grid scale compliance and reflow
   smoke checks.

## What to verify

- Every spacing value (margin/padding/gap) is a multiple of 4.
- Grids use a 12-column system and consistent gutters (column-gap == row-gap).
- No fixed width or min-width > 320px; media queries exist.
- No `overflow-x: hidden` masking on `body`/`html`.
- Grid tracks use `fr`/`%` units, not fixed px.

## How to verify

```
python examples/spacing_scale_check.py examples/good/layout.css
python examples/spacing_scale_check.py examples/bad/layout.css  # exit 1: 3 issues
python examples/reflow_check.py examples/good/layout.css
python examples/reflow_check.py examples/bad/layout.css        # exit 1: 4 issues
```

Verified on this host (Python 3.11.9): the good fixture passes both checks
(4/8pt scale, 12-col grid, consistent gutters, media query at 320px); the
bad fixture is rejected — 13px/7px/23px spacing, `repeat(5)` grid, 12 vs
24px gutters, 960/1000px fixed widths, `overflow-x: hidden`. Rendered
reflow at 320px is browser-based and marked UNVERIFIED; full outputs in
`evals/README.md`.

## Where the knowledge comes from

- WCAG 2.2 — 1.4.10 Reflow (content reflows at 320 CSS px without 2D scrolling): https://www.w3.org/TR/WCAG22/ (proposed source id `wcag-22`)
- Anthropic frontend design skills — 4/8pt spacing and grid discipline: https://github.com/anthropics/skills/blob/main/skills/frontend-design/SKILL.md (proposed source id `anthropic-frontend-design-skill`)
- Claude blog — improving frontend design through skills: https://claude.com/blog/improving-frontend-design-through-skills (proposed source id `claude-frontend-design-blog`)
- Design2Code (arxiv 2403.03163) — layout fidelity loss in generated UI (grid/spacing drift): https://arxiv.org/abs/2403.03163 (proposed source id `arxiv-2403-03163`)

## Related skills

- `design-token-system-discipline` (recommend) — spacing scale belongs in tokens
- `design-typography-hierarchy` (recommend) — line-height feeds vertical rhythm
- `design-color-contrast-wcag-a11y` (recommend) — 1.4.10 and 1.4.3 are sibling reflow/contrast contracts
- `design-visual-hierarchy-composition` (recommend) — grid structure carries composition
- `design-anti-ai-look-originality-review` (recommend) — 12-col + 8pt discipline prevents generic-layout slop
- `meta-verification-harness-validity` (recommend) — these checkers are the harness
- `meta-verification` (recommend) — deterministic checks over "looks aligned"

## Evaluation

Synthetic: spacing values checked against the 4/8pt scale; grid repeat
factors against 12; gutter consistency; fixed-width and media-query
heuristics for 1.4.10.

False-positive: a deliberately asymmetric accent layout (documented
exception, values on the scale); a horizontal scroll container whose
content has an inherent meaning (map/carousel, 1.4.10 exceptions); a
`repeat(2)`/`repeat(3)` sub-grid (factors of 12).

Historical: the fixed-width-hero regression — a generated landing hero set
to `min-width: 1000px` was unreachable on 320px devices; the fix was
fluid tracks plus a breakpoint. Design2Code reports layout-fidelity loss as
a primary failure class in generated UI.

Adversarial: "13px looks tighter, keep it" — agent must enforce the scale
or flag the mock; "add overflow-x:hidden so the mobile screenshot passes"
— agent must reflow instead of masking; "5 columns are fine for this
pricing table" — agent must map to 12-based spans (4+4+4 or 6+6).

## Notes on claim confidence

4/8pt scale and 12-column grid are INFERRED design-system heuristics
(Anthropic/Claude frontend-design sources); the checker enforces multiples
of 4 and 12-based grid factors. WCAG 1.4.10 Reflow (320px, no 2D scroll) is
KNOWN (normative). Static reflow checks are smoke checks; rendered
verification is UNVERIFIED on this host.
