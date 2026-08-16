# Evaluation — design-layout-spacing-grid

Skill: `skills/design/design-layout-spacing-grid`. Stability target:
`evaluated`. Source files: `examples/good/layout.css`,
`examples/bad/layout.css`, `examples/spacing_scale_check.py`,
`examples/reflow_check.py`.

## Host and tooling

Verification host: Windows, Python 3.11.9. Static spacing/grid/reflow
checks are pure-python (verified below). Rendered 320px verification
(DevTools/Playwright/Lighthouse) requires a browser and is documented but
NOT executed here (UNVERIFIED).

## Verification commands and actual output

```
python examples/spacing_scale_check.py examples/good/layout.css
python examples/spacing_scale_check.py examples/bad/layout.css
python examples/reflow_check.py examples/good/layout.css
python examples/reflow_check.py examples/bad/layout.css
```

Observed output:

```
$ python examples/spacing_scale_check.py examples/good/layout.css
examples\good\layout.css: spacing on 4/8pt scale, 12-col grid, consistent gutters
$ echo $LASTEXITCODE
0
$ python examples/spacing_scale_check.py examples/bad/layout.css
ISSUE examples\bad\layout.css: .feature: margin: 13px not on 4/8pt scale
ISSUE examples\bad\layout.css: .grid: grid-template-columns repeat(5) — not a 12-column system factor
ISSUE examples\bad\layout.css: .grid: inconsistent gutters (column-gap 12px vs row-gap 24px)
examples\bad\layout.css: 3 issue(s) found
$ echo $LASTEXITCODE
1
$ python examples/reflow_check.py examples/good/layout.css
examples\good\layout.css: reflow at 320px OK (media query present, no fixed widths)
$ echo $LASTEXITCODE
0
$ python examples/reflow_check.py examples/bad/layout.css
ISSUE examples\bad\layout.css: no @media queries — layout cannot reflow at 320px (WCAG 1.4.10)
ISSUE examples\bad\layout.css: .hero: width: 960px > 320px — breaks reflow at 320px
ISSUE examples\bad\layout.css: .hero: min-width: 1000px > 320px — breaks reflow at 320px
ISSUE examples\bad\layout.css: body: overflow-x:hidden masks reflow problems instead of fixing them
examples\bad\layout.css: 4 reflow issue(s) found
$ echo $LASTEXITCODE
1
```

Documented CI verification (not run here):

```
npx --yes playwright@1.53.0 --help   # or use @playwright/test
# then: viewport {width: 320, height: 256}; assert document.documentElement.scrollWidth <= 320
npx lighthouse <url> --only-categories=accessibility --output=json
```

## Verified facts

| Fact | Status | Evidence |
|---|---|---|
| 4/8pt spacing scale enforced (multiples of 4) | KNOWN (observed) | 13px flagged; token-driven good fixture passes |
| 12-column grid factor enforced | KNOWN (observed) | `repeat(5)` flagged as non-12 factor |
| gutter consistency enforced (column-gap == row-gap) | KNOWN (observed) | 12px vs 24px flagged |
| media-query presence checked (1.4.10 intent) | KNOWN (observed) | no `@media` flagged |
| fixed widths > 320px flagged | KNOWN (observed) | 960px / 1000px flagged |
| `overflow-x:hidden` masking flagged | KNOWN (observed) | body rule flagged |
| rendered reflow at 320px (scrollWidth check) | UNVERIFIED | Playwright/Lighthouse commands documented for CI |

## Synthetic evals

- **easy**: `margin: 13px` — must be flagged (off-scale).
- **easy**: `grid-template-columns: repeat(5, 1fr)` — must be flagged.
- **medium**: `column-gap: 12px; row-gap: 24px` on one grid container —
  must be flagged as inconsistent gutters.
- **medium**: a card with `margin: 8px; padding: 24px;` and a 12-col grid
  with 24px gutters — must pass.
- **hard**: a hero with `min-width: 1000px` plus a comment claiming a
  documented 1.4.10 exception (a data table) — agent must judge whether the
  exception applies to the element it is on.

## False-positive evals (must NOT flag)

- `repeat(2)`, `repeat(3)`, `repeat(6)` sub-grids — factors of 12, correct.
- a horizontal-scrolling carousel/map (2D content has an inherent meaning
  exception under 1.4.10).
- a documented asymmetric accent layout whose values are still multiples of 4.
- `width: 100%` / `min-width: 0` (fluid reset patterns).

## Historical evals

- Fixed-width hero regression: a generated landing page shipped
  `min-width: 1000px` and no breakpoint; on 320px viewports the page
  scrolled horizontally with the primary CTA cut off. Fix: fluid tracks +
  `@media (max-width: 320px)` stacking.
- Design2Code (arxiv 2403.03163) documents layout-fidelity loss as a core
  failure in generated UI — grid/spacing drift is the reported mechanism.

## Adversarial evals

- "13px looks tighter, keep it" — agent must enforce the scale or flag the mock.
- "Add overflow-x:hidden so the mobile screenshot passes" — agent must reflow
  instead of masking.
- "5 columns are fine for this pricing table" — agent must map to 12-based
  spans (4+4+4, 6+6) or justify a nested exception.
- "We tested at 1280px, it's responsive" — agent must demand the 320px
  rendered check.

## Scoring

- detection: names the off-scale value, the non-12 grid factor, the gutter
  inconsistency, or the reflow failure (fixed width / missing breakpoint /
  overflow mask).
- reasoning: explains 4/8pt scale mechanics and the 1.4.10 320px contract.
- fix: moves spacing to the scale (ideally tokens), fixes grid factors and
  gutters, replaces fixed widths with fluid tracks + breakpoints, removes
  the `overflow-x` mask.
- verification: demonstrates checker exit codes; documents the 320px
  rendered check for CI.

## Sources exercised

`wcag-22` (proposed), `anthropic-frontend-design-skill` (proposed),
`claude-frontend-design-blog` (proposed), `arxiv-2403-03163` (proposed),
`lighthouse-a11y-scoring` (proposed). Full reasoning in
`references/spacing-scale-and-grid.md` and `references/reflow-wcag-1410.md`.
