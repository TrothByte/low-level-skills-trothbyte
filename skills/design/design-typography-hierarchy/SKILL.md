---
name: design-typography-hierarchy
description: Use when choosing type sizes, weights, or font stacks, or when reviewing heading structure. Teaches modular type scales and clamp(), display vs body roles, weight contrast, ≤3 sizes, WCAG text spacing, and never skipping heading levels.
---

# Design Typography Hierarchy

## When to use

- Choosing font sizes, weights, line heights, or a font stack for a page or app.
- Reviewing a generated UI whose type ramp came out arbitrary or Inter/Roboto-default.
- Structuring or auditing heading levels (`h1`–`h6`) in HTML.
- Building fluid/type-scale systems (`clamp()`, modular ratios, Utopia-style scales).
- Verifying WCAG text-spacing requirements (line height ≥ 1.5, no letter-spacing traps).

## When not to use

- Verifying color contrast of text — that is `design-color-contrast-wcag-a11y`.
- Tokenizing fonts into a design system — do that via `design-token-system-discipline`.
- Layout/grid and spacing decisions — that is `design-layout-spacing-grid`.
- Judging whether a page "looks generic" — that is `design-anti-ai-look-originality-review`
  (font stack is one of its fingerprints, but it covers much more).

## What the agent often gets wrong

- Emits the default AI ramp: seven arbitrary sizes (34/23/19/15/14/12/11 px)
  with Inter/Roboto/system-ui — an AI-slop tell and a maintenance trap.
- Treats "a font size" as independent of the scale: picks 17px because the
  mock said so, instead of snapping to a deliberate step or `clamp()`.
- Uses light weights only (100/200/300) everywhere, or body weight for
  headings — no weight contrast means no hierarchy when sizes are close.
- Ignores text spacing: `line-height: 1.2` on body text, which fails the
  WCAG 1.4.8/1.4.12 requirement that body text can run at ≥ 1.5 without loss.
- Skips heading levels (`h1` → `h3`) or emits multiple `<h1>`s — an
  accessibility bug that looks like a content decision.
- Uses "System Font Stack" as a shortcut for every project, which also
  fingerprints the output as AI-generated.

## How to reason correctly

1. Pick a base size and a ratio (1.25 major-third, 1.2 minor-third, 1.333
   perfect-fourth; or a Utopia fluid scale) and compute the whole ramp from it —
   never invent each size independently.
2. Assign roles, not just sizes: display (fluid, `clamp()`), heading, body,
   caption. Body is 1rem; headings come from the scale; display is fluid.
3. Give headings weight contrast: 600/700 headings vs 400 body. Never build
   hierarchy from size alone with identical weights.
4. Keep the ramp short (≤ 3 sizes per screen on top of body; long 7+ size
   ramps are a smell) and preserve heading order: exactly one `h1`, levels
   increase by at most one.
5. Verify with the checkers: type-scale extraction, heading-level scan, and
   font-stack scan must all pass.

## What to verify

- Every font size lands on a deliberate modular step or a `clamp()` fluid role.
- ≤ 3 distinct sizes are in active use beyond the base (ramp is short).
- Headings use a heavier weight than body (contrast of 200+ or 600/700 vs 400).
- Body text `line-height` ≥ 1.5 (WCAG 1.4.8 AAA for text blocks; 1.4.12 AA
  requires the layout to survive user overrides to 1.5).
- Exactly one `h1`; no skipped heading levels.
- The first font in every `font-family` is a brand/custom family, not
  Inter/Roboto/system-ui.

## How to verify

```
python examples/type_scale_check.py examples/good/type.css
python examples/type_scale_check.py examples/bad/type.css   # exit 1: 8 issues
python examples/heading_level_check.py examples/good/index.html
python examples/heading_level_check.py examples/bad/index.html  # exit 1: 3 issues
python examples/font_stack_check.py examples/good/type.css
python examples/font_stack_check.py examples/bad/type.css   # exit 1: Inter stack
```

Verified on this host (Python 3.11.9): the good scale (1.25 ratio, base 16px,
clamp display) passes all checks; the bad ramp is rejected with 8 issues
(7 arbitrary sizes, unaligned gaps, light-only weights, line-height 1.2/1.1);
the bad HTML has 2 `<h1>`s and two skips; the Inter stack is flagged. Full
outputs in `evals/README.md`.

## Where the knowledge comes from

- WCAG 2.2 — 1.4.8 Visual Presentation (line spacing ≥ 1.5) and 1.4.12 Text Spacing: https://www.w3.org/TR/WCAG22/ (proposed source id `wcag-22`)
- NN/g — visual hierarchy and typography guidance: https://www.nngroup.com/articles/visual-hierarchy-ux-definition/ (proposed source id `nngroup-visual-hierarchy`)
- Modular scale / fluid type calculators: https://www.modularscale.com/ and https://utopia.fyi/calculator (proposed source ids `modular-scale`, `utopia-fluid-type`)
- Anthropic frontend design skills — type scale and weight guidance: https://github.com/anthropics/skills/blob/main/skills/frontend-design/SKILL.md (proposed source id `anthropic-frontend-design-skill`)

## Related skills

- `design-token-system-discipline` (recommend) — type sizes must live in tokens, not literals
- `design-color-contrast-wcag-a11y` (recommend) — text size interacts with contrast thresholds (large-text 3:1)
- `design-layout-spacing-grid` (recommend) — line height feeds vertical rhythm
- `design-visual-hierarchy-composition` (recommend) — heading order and size hierarchy are half of composition
- `design-anti-ai-look-originality-review` (recommend) — font stack is an AI-look fingerprint
- `meta-verification-harness-validity` (recommend) — these checkers are the harness; validate they fail on real defects
- `meta-verification` (recommend) — evidence over vibes for "the type scale is deliberate"

## Evaluation

Synthetic: good vs bad type ramps (scale conformance, size count, weight
contrast, line-height); heading structures (one h1, no skips); font-stack
fingerprints.

False-positive: legitimate design choices that are not defects — a caption
at 0.8rem, a fluid display `clamp()` that exceeds the scale, a heading
`line-height: 1.2`, a brand family (e.g. "Spline Sans") that leads its stack.

Historical: known AI-slop regression — a generated page that switched the
headline to 34px light-weight Inter "to look premium" and dropped body
line-height to 1.2; the ramp was rewritten from the scale, not tuned
size-by-size.

Adversarial: "just one more size for this statistic" — agent must fit it to
the scale or refuse; "the mock uses 17px, keep it" — agent must snap to the
scale and flag the mock; "add a light 100-weight tagline" — agent must pair
it with a heavy counterpart or justify.

## Notes on claim confidence

"Body line-height ≥ 1.5" is KNOWN from WCAG 1.4.8 (text blocks, AAA) and
1.4.12 (user-override survival, AA). "≤ 3 sizes per screen" is an INFERRED
heuristic attributed to NN/g style guidance (the checker enforces a loose
limit of 7, treating long ramps as the smell, not exactly 3).
