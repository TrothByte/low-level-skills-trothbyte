# Spacing Scale and Grid Discipline

Rule format: RULE / WHY AI GETS IT WRONG / CORRECT REASONING / EXAMPLE /
COUNTEREXAMPLE / VERIFICATION / SOURCE. Facts marked **VERIFIED** were
observed on this host (Python 3.11.9) with the fixtures under `examples/`.

## 1. Spacing values live on a 4/8pt scale

- **RULE**: all margins, paddings, and gaps are multiples of 4 (4/8pt
  system: 4, 8, 12, 16, 24, 32, 48). Values like 13px, 7px, 23px are
  defects: they belong to no scale and cannot be themed.
- **WHY AI GETS IT WRONG**: the agent reproduces pixel values from a mock
  ("13px looks tighter"), so the layout has a different spacing constant
  for every element and no rhythm.
- **CORRECT REASONING**: a spacing scale makes layout computable and
  reviewable, like a type scale. An off-scale value is a greppable defect.
  If the mock disagrees with the scale, the scale wins or the mock is wrong.
- **EXAMPLE**: `.card { margin: var(--space-8); padding: var(--space-24); }`
  with the scale defined once in `:root`.
- **COUNTEREXAMPLE**: `.feature { margin: 13px; padding: 7px 23px; }`.
- **VERIFICATION**: VERIFIED — `spacing_scale_check.py` flags 13px on
  `.feature` (exit 1) and accepts the token-driven good fixture (exit 0).
- **SOURCE**: https://github.com/anthropics/skills/blob/main/skills/frontend-design/SKILL.md (proposed `anthropic-frontend-design-skill`); https://claude.com/blog/improving-frontend-design-through-skills (proposed `claude-frontend-design-blog`)

## 2. One grid system: 12 columns, consistent gutters

- **RULE**: use a 12-column grid with a single gutter (column-gap == row-gap).
  Section layouts use factor widths (12, 6, 4, 3, 2) — never 5 or 7 columns
  outside an explicit nested exception.
- **WHY AI GETS IT WRONG**: the agent invents `repeat(5, 1fr)` for a pricing
  row and `column-gap: 12px; row-gap: 24px` in the same container; the page
  has no grid relationship between sections.
- **CORRECT REASONING**: a 12-column system gives every width a mathematical
  relationship. Gutter inconsistency produces misalignment that screenshots
  average out but the rendered page shows at every breakpoint.
- **EXAMPLE**: `grid-template-columns: repeat(12, 1fr); column-gap: var(--gutter); row-gap: var(--gutter);`.
- **COUNTEREXAMPLE**: `grid-template-columns: repeat(5, 1fr); column-gap: 12px; row-gap: 24px;`.
- **VERIFICATION**: VERIFIED — the checker reports `repeat(5) — not a
  12-column system factor` and `inconsistent gutters (column-gap 12px vs
  row-gap 24px)` for the bad fixture (exit 1).
- **SOURCE**: https://claude.com/blog/improving-frontend-design-through-skills (proposed `claude-frontend-design-blog`); https://arxiv.org/abs/2403.03163 (proposed `arxiv-2403-03163`)

## 3. Grid tracks are fluid, never fixed px

- **RULE**: grid and layout tracks use `fr`, `%`, or `clamp()` units. Fixed
  px tracks (e.g. `grid-template-columns: 200px 200px 200px`) break reflow
  and violate the 320px contract.
- **WHY AI GETS IT WRONG**: fixed px tracks render identically to fluid ones
  at desktop width, so the agent ships them and the mobile breakpoint
  overflows.
- **CORRECT REASONING**: fluid tracks are the mechanism that makes 1.4.10
  achievable. If a track must be fixed, it needs an explicit exception and
  a wrapping behavior at 320px.
- **EXAMPLE**: `grid-template-columns: repeat(12, 1fr);`.
- **COUNTEREXAMPLE**: `grid-template-columns: repeat(3, 250px);`.
- **VERIFICATION**: VERIFIED smoke check — `reflow_check.py` flags fixed-px
  grid tracks and widths > 320px; rendered confirmation is browser-based
  (UNVERIFIED here).
- **SOURCE**: https://www.w3.org/TR/WCAG22/ — 1.4.10 (proposed `wcag-22`); https://claude.com/blog/improving-frontend-design-through-skills (proposed `claude-frontend-design-blog`)
