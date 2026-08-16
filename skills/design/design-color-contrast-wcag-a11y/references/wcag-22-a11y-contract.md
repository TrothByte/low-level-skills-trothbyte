# WCAG 2.2 as a Design Contract

Rule format: RULE / WHY AI GETS IT WRONG / CORRECT REASONING / EXAMPLE /
COUNTEREXAMPLE / VERIFICATION / SOURCE. Facts marked **VERIFIED** were
observed on this host (Python 3.11.9) with the fixtures under `examples/`.

## 1. Structure is part of the contract: lang, title, alt, labels

- **RULE**: the page contract includes `<html lang>` (3.1.1), exactly one
  `<title>` (2.4.2), an `alt` attribute on every `<img>` (1.1.1, empty
  `alt=""` is correct for decorative images), and an accessible label on
  every form control (1.3.1, 4.1.2).
- **WHY AI GETS IT WRONG**: visual review passes — the page "looks like a
  real site" — so the missing lang/title/alt/labels never surface in an
  agent's self-check, which is dominated by pixels.
- **CORRECT REASONING**: these are declarative, greppable properties. Check
  them with a script; they are the cheapest a11y wins and the most
  frequently missed.
- **EXAMPLE**: `<html lang="en">`, `<title>Sign in — Atlas</title>`,
  `<img src="logo.svg" alt="Atlas logo">`, `<label for="email">Email</label>`.
- **COUNTEREXAMPLE**: `<html>` (no lang), no title, `<img src="chart.png">`,
  `<input placeholder="Email">` with no label.
- **VERIFICATION**: VERIFIED — `a11y_attributes_check.py` reports 6 issues
  for the bad fixture (lang, title, 2× alt, 2× labels) and OK for the good
  one (exit 0).
- **SOURCE**: https://www.w3.org/TR/WCAG22/ — 1.1.1, 1.3.1, 2.4.2, 3.1.1 (proposed `wcag-22`); https://webaim.org/projects/million/ (proposed `webaim-million`)

## 2. Interactive targets are at least 24×24 CSS px (WCAG 2.2 AA)

- **RULE**: every interactive target (links, buttons, inputs, tabs) must be
  ≥ 24×24 CSS px with no more than 24px of spacing to an adjacent target
  (2.5.8 Target Size Minimum). 44×44 is the AAA target (2.5.5) and a good
  default for primary actions.
- **WHY AI GETS IT WRONG**: the agent sizes buttons to the text and ships a
  16×16 submit control; nothing in the screenshot signals the failure.
- **CORRECT REASONING**: target size is arithmetic. If a target has explicit
  dimensions, check them; if it uses padding, estimate hit area from
  padding + glyph box. Either way, the number must reach 24.
- **EXAMPLE**: `.button { min-width: 24px; min-height: 24px; }`; a link with
  `padding: 6px 12px` at 16px font → ~31px tall hit area.
- **COUNTEREXAMPLE**: `input.submit { width: 16px; height: 16px; }`, a tab at
  20×20.
- **VERIFICATION**: VERIFIED — `contrast_check.py` flags the 20×20 tab and
  16×16 submit (exit 1) and accepts the 24×24 button and the padding-based
  link estimate (~31px) in the good fixture.
- **SOURCE**: https://www.w3.org/TR/WCAG22/ — 2.5.8, 2.5.5 (proposed `wcag-22`); https://developer.chrome.com/docs/lighthouse/accessibility/scoring/ (proposed `lighthouse-a11y-scoring`)

## 3. Automate with axe-core and Lighthouse in CI

- **RULE**: add automated checks to the pipeline: axe-core (`npx @axe-core/cli`
  with WCAG 2.2 tags) and Lighthouse accessibility category. The design
  contract is enforced by tools, not by code review.
- **WHY AI GETS IT WRONG**: the agent runs no checks and reports "verified"
  from a screenshot; browser-based scanners catch the exact failure classes
  this skill enumerates.
- **CORRECT REASONING**: static Python checks cover luminance math and
  attribute presence; axe/Lighthouse cover rendered behavior (focus order,
  ARIA, contrast in the rendered cascade). Run both where a browser exists.
- **EXAMPLE**: `npx --yes @axe-core/cli http://localhost:3000 --tags wcag2a,wcag2aa,wcag22aa`.
- **COUNTEREXAMPLE**: "we tested in Chrome and it looks fine" with no tool
  run and no exit code.
- **VERIFICATION**: UNVERIFIED on this host — commands documented exactly;
  no browser is available here. Marked for the integration environment.
- **SOURCE**: https://developer.chrome.com/docs/lighthouse/accessibility/scoring/ (proposed `lighthouse-a11y-scoring`); https://arxiv.org/abs/2501.03572 (proposed `arxiv-2501-03572`)
