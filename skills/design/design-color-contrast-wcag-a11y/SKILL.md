---
name: design-color-contrast-wcag-a11y
description: Use when choosing color pairs or verifying accessibility of text, UI, and images. Teaches WCAG 2.2 as a design contract: relative luminance and contrast math, 4.5:1/3:1 thresholds, alt/labels/lang, 24px targets, axe-core/Lighthouse verification.
---

# Design Color Contrast & WCAG 2.2 Accessibility

## When to use

- Choosing or reviewing color pairs for text, large text, or UI components.
- Verifying a page against WCAG 2.2 AA: contrast, target size, alt text,
  labels, lang, page title.
- Auditing a generated UI that uses low-contrast gray-on-white or brand-color-on-brand-color.
- Adding automated accessibility checks (axe-core, Lighthouse) to a build.
- Deciding whether a color pair passes without guessing — computing the ratio.

## When not to use

- Choosing type scale/weights — that is `design-typography-hierarchy`.
- Tokenizing the palette — `design-token-system-discipline` owns token structure.
- Layout/grid and 320px reflow — `design-layout-spacing-grid` (1.4.10 Reflow
  is close, but this skill is color/text-centric).
- Full formal accessibility audit of a complex app — this covers the design
  contract; a complete audit also needs interaction and keyboard patterns.

## What the agent often gets wrong

- Claims "that contrast is fine" by eyeballing it instead of computing
  relative luminance — the most common self-eval divergence in accessibility.
- Picks #FF0000 red or #999999 gray for text on white, which visually
  "match" but fail the 4.5:1 requirement (4.0:1 and 2.85:1 respectively).
- Uses 3:1 as the universal threshold — 3:1 applies to large text (≥18pt or
  ≥14pt bold) and UI/graphics; normal text needs 4.5:1.
- Leaves out `alt`, `lang`, `title`, and labels — AI-slop HTML that passes
  visual review but fails WCAG 1.1.1/3.1.1/2.4.2/1.3.1.
- Ships interactive targets under 24×24 CSS px (WCAG 2.2 2.5.8), e.g. a
  16px submit button, and ignores target-size checks.
- Trusts a screenshot instead of running axe-core/Lighthouse.

## How to reason correctly

1. Compute WCAG contrast as a number: sRGB channel → linearize with the
   piecewise formula, relative luminance L = 0.2126R + 0.7152G + 0.0722B,
   ratio = (Lmax + 0.05) / (Lmin + 0.05). Never eyeball it.
2. Apply the right threshold per use: normal text 4.5:1, large text 3:1,
   UI components/graphics 3:1 (WCAG 2.2 1.4.3/1.4.11).
3. Treat the page contract as including structure: `<html lang>`, one
   `<title>`, `alt` on every `<img>`, and an accessible label for every
   form control.
4. Give every interactive target ≥ 24×24 CSS px (WCAG 2.2 2.5.8); larger
   (44×44, 2.5.5 AAA) where possible.
5. Automate: run the contrast script and a11y attribute script locally, and
   run axe-core/Lighthouse in CI where a browser is available.

## What to verify

- Every text pair ≥ 4.5:1 (normal) or ≥ 3:1 (large ≥18pt or ≥14pt bold).
- Every UI/graphic pair ≥ 3:1.
- The ratio is computed from WCAG relative luminance, not perceptual guess.
- `<html lang>` present, exactly one `<title>`, every `<img>` has `alt`.
- Every form control has an accessible label (label-for/aria-label).
- Interactive targets ≥ 24×24 CSS px.

## How to verify

```
python examples/contrast_check.py examples/good/palette.css
python examples/contrast_check.py examples/bad/palette.css   # exit 1: 3 FAIL + 2 targets
python examples/a11y_attributes_check.py examples/good/page.html
python examples/a11y_attributes_check.py examples/bad/page.html   # exit 1: 6 issues
npx --yes @axe-core/cli <url> --tags wcag2a,wcag2aa,wcag22aa
npx lighthouse <url> --only-categories=accessibility --output=json
```

Verified on this host (Python 3.11.9): contrast ratios are computed exactly
(17.74:1 for #111827/#FFF; 4.00:1 for #F00/#FFF — fails normal text;
2.85:1 for #999/#FFF; 1.61:1 for #CCC/#FFF — fails UI); the bad target
sizes (20×20, 16×16) are flagged. axe-core and Lighthouse need a real
browser/URL — commands above are documented, NOT executed here. Full
outputs in `evals/README.md`.

## Where the knowledge comes from

- WCAG 2.2 — 1.4.3 Contrast (Minimum), 1.4.11 Non-text Contrast, 1.4.1 Use of Color, 2.5.8 Target Size (Minimum), 1.1.1, 3.1.1, 2.4.2, 1.3.1: https://www.w3.org/TR/WCAG22/ (proposed source id `wcag-22`)
- WebAIM Contrast Checker (luminance/contrast math): https://webaim.org/resources/contrastchecker/ (proposed source id `webaim-contrast-api`)
- WebAIM Million (most common home-page failures are low contrast): https://webaim.org/projects/million/ (proposed source id `webaim-million`)
- Lighthouse accessibility scoring: https://developer.chrome.com/docs/lighthouse/accessibility/scoring/ (proposed source id `lighthouse-a11y-scoring`)
- From Code to Compliance (arxiv 2501.03572) — LLM-generated code passes visual but misses a11y contract: https://arxiv.org/abs/2501.03572 (proposed source id `arxiv-2501-03572`)

## Related skills

- `design-token-system-discipline` (recommend) — semantic color tokens must still satisfy contrast
- `design-typography-hierarchy` (recommend) — large-text threshold depends on size/weight
- `design-layout-spacing-grid` (recommend) — 1.4.10 Reflow pairs with these checks
- `design-visual-hierarchy-composition` (recommend) — contrast is one axis of hierarchy
- `design-anti-ai-look-originality-review` (recommend) — default palettes fail both aesthetics and a11y
- `meta-verification-harness-validity` (recommend) — the checkers are the harness; they must fail on real defects
- `meta-verification` (recommend) — computed ratio beats "looks accessible"

## Evaluation

Synthetic: color pairs computed against 4.5/3.0/3.0 thresholds; large-text
classification (18pt / 14pt bold); target-size arithmetic; HTML attribute
presence (lang/title/alt/labels).

False-positive: a decorative image with `alt=""` and `aria-hidden` is
correct, not a defect; a 44×44 target for a critical action is better than
required; `#4F46E5 on #F5F7FF` at 5.88:1 passes 3:1 for UI — must not be
flagged.

Historical: WebAIM Million — low contrast is the single most common
home-page accessibility failure across 1M pages; a color-pair audit of a
generated landing page reproduced the failure class (red/gray on white).

Adversarial: "these grays look fine on my monitor" — agent must compute the
ratio and show the number; "3:1 is fine, we checked" for 16px body text —
agent must apply the 4.5:1 rule; "it's just a decorative icon" — agent must
still check 1.4.11/1.1.1; "the contrast script is a nice-to-have" — agent
must run it and record exit codes.

## Notes on claim confidence

Thresholds and luminance math are KNOWN (WCAG 2.2 normative). The 24×24
target minimum is KNOWN (2.5.8, WCAG 2.2 AA). axe-core/Lighthouse runs are
UNVERIFIED on this host (no browser) and documented as exact commands.
