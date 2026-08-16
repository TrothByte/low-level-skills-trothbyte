# WCAG 1.4.10 Reflow at 320px

Rule format: RULE / WHY AI GETS IT WRONG / CORRECT REASONING / EXAMPLE /
COUNTEREXAMPLE / VERIFICATION / SOURCE. Facts marked **VERIFIED** were
observed on this host (Python 3.11.9) with the fixtures under `examples/`.

## 1. Content must reflow at 320 CSS px without 2D scrolling

- **RULE**: WCAG 1.4.10 (Reflow, AA) requires that content reflows to a
  width of 320 CSS px (256 px height landscape) without two-dimensional
  scrolling — except for content that needs a 2D layout for meaning (tables,
  maps, diagrams).
- **WHY AI GETS IT WRONG**: the agent designs at desktop width only; a fixed
  `min-width: 1000px` hero looks fine in the mock and fails every phone.
- **CORRECT REASONING**: 320px is the floor. Fluid units, breakpoints, and
  stacked layouts are the mechanism; if a page scrolls sideways at 320px,
  it fails AA by definition.
- **EXAMPLE**: `.layout { grid-template-columns: repeat(12, 1fr); }` plus
  `@media (max-width: 320px) { .layout { grid-template-columns: 1fr; } }`.
- **COUNTEREXAMPLE**: `.hero { min-width: 1000px; width: 960px; }` with no
  breakpoint.
- **VERIFICATION**: VERIFIED smoke check — `reflow_check.py` reports the
  absence of `@media` and the 960/1000px widths for the bad fixture (4
  issues, exit 1). Rendered confirmation is browser-based (UNVERIFIED here).
- **SOURCE**: https://www.w3.org/TR/WCAG22/ — 1.4.10 (proposed `wcag-22`)

## 2. `overflow-x: hidden` is masking, not fixing

- **RULE**: never "fix" reflow by clipping with `overflow-x: hidden` on
  `body`/`html`. If content overflows, the fix is a fluid layout; clipping
  hides the failure from users and from tooling.
- **WHY AI GETS IT WRONG**: the agent adds `overflow-x: hidden` to make the
  page stop scrolling sideways — it passes the eyeball check and produces a
  cutoff layout on small screens.
- **CORRECT REASONING**: clipping removes the scrollbar but not the
  overflow; content is still unreachable. The checker should treat it as a
  defect, and the fix must reflow the tracks.
- **EXAMPLE**: fluid tracks + a 320px breakpoint (no `overflow-x` hack).
- **COUNTEREXAMPLE**: `body { overflow-x: hidden; }` alongside
  `min-width: 1000px` elements.
- **VERIFICATION**: VERIFIED — `reflow_check.py` flags `body:
  overflow-x:hidden` in the bad fixture.
- **SOURCE**: https://www.w3.org/TR/WCAG22/ — 1.4.10 (proposed `wcag-22`); https://developer.chrome.com/docs/lighthouse/accessibility/scoring/ (proposed `lighthouse-a11y-scoring`)

## 3. Reflow is a layout property, verified in the rendered cascade

- **RULE**: static checks prove intent (media queries exist, no fixed
  widths); the rendered check at 320px (DevTools viewport, Playwright
  viewport resize) proves the result. Document both.
- **WHY AI GETS IT WRONG**: the agent declares "responsive" because the CSS
  contains an `@media` block — the breakpoint may break nothing relevant.
- **CORRECT REASONING**: the property to verify is "no horizontal overflow
  at 320px", which is a rendered measurement. Static heuristics are smoke
  gates; the real gate is the viewport test in CI.
- **EXAMPLE**: Playwright `page.setViewportSize({width: 320, height: 256})`
  then `document.documentElement.scrollWidth <= 320`.
- **COUNTEREXAMPLE**: a claim of reflow support backed only by "we have a
  media query".
- **VERIFICATION**: UNVERIFIED on this host — the Playwright/Lighthouse
  commands are documented in `evals/README.md` for the CI environment.
- **SOURCE**: https://www.w3.org/TR/WCAG22/ — 1.4.10 (proposed `wcag-22`); https://arxiv.org/abs/2403.03163 (proposed `arxiv-2403-03163`)
