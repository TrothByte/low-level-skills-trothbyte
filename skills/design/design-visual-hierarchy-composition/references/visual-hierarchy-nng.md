# NN/g Visual Hierarchy

Rule format: RULE / WHY AI GETS IT WRONG / CORRECT REASONING / EXAMPLE /
COUNTEREXAMPLE / VERIFICATION / SOURCE. Facts marked **VERIFIED** were
observed on this host (Python 3.11.9) with the fixtures under `examples/`.

## 1. Exactly one h1, and heading levels never skip

- **RULE**: a page has exactly one `<h1>`; heading levels increase by at
  most one (`h1` → `h2` → `h3`). Multiple h1s and skipped levels break the
  document outline that assistive tech and search engines rely on.
- **WHY AI GETS IT WRONG**: sections are generated independently and
  concatenated, so "Features" becomes `h3` and "Pricing" becomes `h5`
  while two `h1`s sit at the top — the outline is garbage but the page
  "looks fine".
- **CORRECT REASONING**: hierarchy is a structure, not a vibe. Heading
  order is mechanically checkable; a skipped level or a second h1 is a
  defect, regardless of how the page renders.
- **EXAMPLE**: `h1` (Atlas railway analytics) → `h2` (Fleet health) →
  `h3` (Engine telemetry).
- **COUNTEREXAMPLE**: `h1` Welcome, `h1` Our Company, then `h3`, then `h5`.
- **VERIFICATION**: VERIFIED — `composition_check.py` reports two `<h1>`s
  and both skips for the bad fixture (exit 1) and OK for the good one
  (exit 0).
- **SOURCE**: https://www.w3.org/TR/WCAG22/ — 1.3.1 (proposed `wcag-22`); https://www.nngroup.com/articles/visual-hierarchy-ux-definition/ (proposed `nngroup-visual-hierarchy`)

## 2. Key content lives in the first two paragraphs

- **RULE**: the first paragraph states the page's thesis and the second
  supports it. Users scan; content that matters past the first two
  paragraphs is content that does not get read by most visitors.
- **WHY AI GETS IT WRONG**: the agent fills the top of the page with
  welcoming filler or lorem ipsum and puts the real value statement in
  paragraph five — the canonical buried-thesis failure.
- **CORRECT REASONING**: treat the first screenful as the promise. If the
  page's core terms (its `<title>` keywords) do not appear in the first two
  paragraphs, the page is not leading with its point.
- **EXAMPLE**: h1 "Atlas railway analytics" → p1 "Atlas turns raw train
  telemetry into delay predictions…" → p2 "This page covers the two
  dashboards…".
- **COUNTEREXAMPLE**: two lorem paragraphs, then a third, with the actual
  company pitch in paragraph five.
- **VERIFICATION**: VERIFIED — the checker requires a shared significant
  `<title>` keyword in the first two paragraphs; the bad fixture fails
  ("key content not in the first two paragraphs"), the good one passes.
- **SOURCE**: https://www.nngroup.com/articles/visual-hierarchy-ux-definition/ (proposed `nngroup-visual-hierarchy`); https://www.nngroup.com/articles/f-shaped-pattern-reading-web-content/ (proposed `nngroup-f-pattern`)

## 3. Hierarchy is rank, established by contrast

- **RULE**: rank content into 3–4 levels (thesis, sections, supporting,
  chrome) and give each a distinct visual weight via size, weight, color,
  or spacing. Equal weight everywhere = flat hierarchy.
- **WHY AI GETS IT WRONG**: generated pages assign the same heading size,
  color, and spacing to every block; the squint test collapses everything
  into one gray mass.
- **CORRECT REASONING**: visual hierarchy is what lets users skip. The
  squint test (step back / blur until detail disappears) shows whether
  weight is ranked; if only 1–2 levels survive, the design is flat.
- **EXAMPLE**: a 31px h1, 25px h2, 16px body, muted caption — four clear
  levels.
- **COUNTEREXAMPLE**: h1 20px, h2 19px, body 16px, all weight 400 — two
  levels of gray.
- **VERIFICATION**: INFERRED — rank contrast is a rendering property; the
  static checker verifies structure and thesis placement, while weight
  contrast is machine-checked by `design-typography-hierarchy`'s checker.
- **SOURCE**: https://www.nngroup.com/articles/visual-hierarchy-ux-definition/ (proposed `nngroup-visual-hierarchy`); https://github.com/anthropics/skills/blob/main/skills/frontend-design/SKILL.md (proposed `anthropic-frontend-design-skill`)
