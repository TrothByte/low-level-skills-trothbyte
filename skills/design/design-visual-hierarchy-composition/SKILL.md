---
name: design-visual-hierarchy-composition
description: Use when structuring a page or evaluating hierarchy and composition. Teaches NN/g visual hierarchy: exactly one h1, heading order, squint test, F-pattern, hero-as-thesis, and key content in the first two paragraphs.
---

# Design Visual Hierarchy & Composition

## When to use

- Structuring a page: heading order, hero copy, primary CTA placement.
- Reviewing a generated page where everything has equal visual weight.
- Evaluating whether the most important content is above the fold and in
  the first two paragraphs.
- Checking single-h1, heading-order, and thesis-first composition rules.
- Applying the squint test and F-pattern reasoning to a layout.

## When not to use

- Contrast/color hierarchy — that is `design-color-contrast-wcag-a11y`.
- Type scale/weights — `design-typography-hierarchy` owns the type ramp.
- Grid/spacing mechanics — `design-layout-spacing-grid`.
- Originality/brand review ("does it look AI-generated") — that is
  `design-anti-ai-look-originality-review`.

## What the agent often gets wrong

- Gives every section equal visual weight: same size, same color, same
  spacing — a "flat hierarchy" page that forces the user to read everything.
- Emits two `<h1>`s or skips heading levels (`h1`→`h3`) because each section
  was generated independently.
- Buries the thesis: five paragraphs of filler before the actual point
  (lorem ipsum at the top is the canonical tell).
- Leads with the CTA/nav instead of the value proposition (hero-as-thesis
  inversion).
- Assumes "content" placement without considering F-pattern scanning:
  important text lands mid-page or right-aligned where users never look.
- Reports "hierarchy is fine" from a desktop screenshot — the squint test
  and 320px view both disagree.

## How to reason correctly

1. Start from exactly one `<h1>` that states the page's purpose (thesis),
  then `h2`/`h3` sections in order — never skip levels, never a second h1.
2. Put the thesis in the first paragraph and supporting detail in the
  second: key content in the first two paragraphs, per NN/g scanning
  behavior.
3. Establish the hero as the thesis: h1 + one-paragraph value statement +
  a single primary CTA before any body content.
4. Use contrast (size, weight, color, spacing) to rank elements, and verify
  with the squint test: blur/step back until only 3–4 levels of visual
  weight remain; if everything is equal, hierarchy is flat.
5. Place the most important content in the top-left scanning path
  (F-pattern): first paragraph, CTA, and first heading before the fold.

## What to verify

- Exactly one `<h1>`; heading levels never skip.
- The first paragraph states the thesis (shares the page's key terms).
- Key content appears within the first two paragraphs.
- The first paragraph/hero sits near the top of the document (before the
  middle), not buried.
- Squint test: ≤ 3–4 distinct visual-weight levels are visible.
- Primary CTA precedes the body sections.

## How to verify

```
python examples/composition_check.py examples/good/index.html
python examples/composition_check.py examples/bad/index.html  # exit 1: 5 issues
```

Verified on this host (Python 3.11.9): the good page passes (4 headings,
one h1, thesis in the first paragraph); the bad page is rejected — two
`<h1>`s, two heading skips, and thesis/keywords missing from the first two
paragraphs. The squint test and eye-tracking F-pattern are manual/browser
methods — documented, not automated here. Full outputs in `evals/README.md`.

## Where the knowledge comes from

- NN/g — Visual Hierarchy: https://www.nngroup.com/articles/visual-hierarchy-ux-definition/ (proposed source id `nngroup-visual-hierarchy`)
- NN/g — F-Shaped Pattern of Reading on the Web: https://www.nngroup.com/articles/f-shaped-pattern-reading-web-content/ (proposed source id `nngroup-f-pattern`)
- WCAG 2.2 — 1.3.1 Info and Relationships (heading structure): https://www.w3.org/TR/WCAG22/ (proposed source id `wcag-22`)
- Anthropic frontend design skills — hierarchy and composition guidance: https://github.com/anthropics/skills/blob/main/skills/frontend-design/SKILL.md (proposed source id `anthropic-frontend-design-skill`)

## Related skills

- `design-typography-hierarchy` (recommend) — size/weight contrast is the raw material of hierarchy
- `design-color-contrast-wcag-a11y` (recommend) — color contrast is a hierarchy axis
- `design-layout-spacing-grid` (recommend) — grid position and whitespace create weight
- `design-anti-ai-look-originality-review` (recommend) — flat generic composition is an AI-look marker
- `meta-verification-harness-validity` (recommend) — the checker is the harness
- `meta-verification` (recommend) — the squint test is manual; the checkable parts must be checked

## Evaluation

Synthetic: single-h1 enforcement; heading-order no-skip; first-two-paragraph
thesis keyword presence; hero position in document order.

False-positive: a document whose first paragraph is a legitimate kicker or
eyebrow when the page has no `<title>` keyword to align with; a very short
hero statement (< 40 chars) that is intentionally telegraphic; an
`aria-label`-based single-page app that uses `role="heading"` attributes
instead of `h1–h6` tags (the checker targets authored HTML).

Historical: NN/g F-pattern findings — users scan the first line of body
copy and the top-left region; a redesign that moved the thesis from
paragraph five to paragraph one measurably improved task success. The bad
fixture reproduces the buried-thesis failure class.

Adversarial: "every section is equally important" — agent must force a
ranking (thesis > section > supporting); "add a second h1 for the brand" —
agent must route brand into the single h1 or an `<h2>`/logo role; "the
lorem ipsum will be replaced" — agent must not ship placeholder as the
thesis; "users read everything anyway" — agent must invoke F-pattern
evidence.

## Notes on claim confidence

Single-h1 and no-skip heading order are KNOWN (WCAG 1.3.1 structure; also
documented hierarchy guidance). First-two-paragraph thesis placement and
F-pattern are INFERRED from NN/g scanning research (documented, not
eye-tracking-verified here). The squint test is a manual method, marked
UNVERIFIED as an automated check.
