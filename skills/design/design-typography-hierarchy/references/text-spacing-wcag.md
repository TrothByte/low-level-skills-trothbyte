# Text Spacing and Heading Structure (WCAG)

Rule format: RULE / WHY AI GETS IT WRONG / CORRECT REASONING / EXAMPLE /
COUNTEREXAMPLE / VERIFICATION / SOURCE. Facts marked **VERIFIED** were
observed on this host (Python 3.11.9) with the fixtures under `examples/`.

## 1. Body text line-height at least 1.5

- **RULE**: set body-text line-height ≥ 1.5. WCAG 1.4.8 (Visual
  Presentation, AAA) requires line spacing ≥ 1.5 within paragraphs; WCAG
  1.4.12 (Text Spacing, AA) requires the layout to survive a user override
  of line-height to 1.5, letter-spacing to 0.12em, and word-spacing to
  0.16em without loss of content.
- **WHY AI GETS IT WRONG**: compact line-heights (1.2, 1.3) look "tight" in
  a screenshot and pass the AI visual check, but they break the spacing
  requirement and make text harder to read.
- **CORRECT REASONING**: if the design cannot tolerate line-height 1.5,
  it fails 1.4.12 by construction. Tight leading is reserved for display and
  headings where it is a design decision, not for body copy.
- **EXAMPLE**: `body { line-height: 1.6; }`, caption 1.5, heading 1.2
  (allowed — 1.4.8/1.4.12 target text blocks).
- **COUNTEREXAMPLE**: `body { line-height: 1.2; }`, `.button { line-height: 1.1; }`.
- **VERIFICATION**: VERIFIED — `type_scale_check.py` flags `line-height 1.2`
  and `1.1` on non-heading selectors (exit 1) and accepts the good fixture
  (body 1.6, caption 1.5, headings exempt).
- **SOURCE**: https://www.w3.org/TR/WCAG22/ — 1.4.8, 1.4.12 (proposed `wcag-22`)

## 2. Exactly one h1 and no skipped heading levels

- **RULE**: a page/document has exactly one `<h1>`; heading levels increase
  by at most one (no `h1`→`h3` jumps, no `h3`→`h5` jumps). First heading
  must be `h1`.
- **WHY AI GETS IT WRONG**: agents compose sections independently ("Features
  = h3", "Pricing = h5") and concatenate them, producing skipped levels and
  duplicate h1s that look fine in isolation.
- **CORRECT REASONING**: heading order is assistive-tech navigation. Skipping
  levels is a WCAG structure violation (heading/section consistency), and
  multiple h1s break the document outline. It is mechanically checkable, so
  check it.
- **EXAMPLE**: `h1` → `h2` (Fleet health) → `h3` (Engine telemetry) → `h2` → `h3`.
- **COUNTEREXAMPLE**: two `<h1>`s, then `h3`, then `h5`.
- **VERIFICATION**: VERIFIED — `heading_level_check.py` reports 3 issues for
  the bad fixture (2 h1s + 2 skips, exit 1) and OK for the good one (4
  headings, exactly one h1, no skips, exit 0).
- **SOURCE**: https://www.w3.org/TR/WCAG22/ — 1.3.1 Info and Relationships (proposed `wcag-22`); https://www.nngroup.com/articles/visual-hierarchy-ux-definition/ (proposed `nngroup-visual-hierarchy`)

## 3. Type and text-spacing must not fight responsive layout

- **RULE**: fluid type (`clamp(min, preferred, max)`) is the deliberate way
  to scale type across viewports; fixed px-only ramps break hierarchy on
  small screens and produce overflow.
- **WHY AI GETS IT WRONG**: the agent ships fixed px everywhere; at 320px the
  h1 overflows or wraps awkwardly, and no test catches it because the
  screenshot was taken at desktop width.
- **CORRECT REASONING**: clamp() keeps the display role expressive at desktop
  and safe at 320px; the rest of the ramp stays on the scale. Combine with
  `design-layout-spacing-grid` reflow checks.
- **EXAMPLE**: `--font-display: clamp(2.5rem, 6vw, 3.5rem);`.
- **COUNTEREXAMPLE**: `h1 { font-size: 48px; }` with no fluid behavior.
- **VERIFICATION**: INFERRED for the 320px behavior here (browser-rendering
  based); the type-scale checker accepts clamp() as a fluid display role and
  rejects px-only arbitrary ramps.
- **SOURCE**: https://utopia.fyi/calculator (proposed `utopia-fluid-type`); https://www.w3.org/TR/WCAG22/ — 1.4.10 Reflow (proposed `wcag-22`)
