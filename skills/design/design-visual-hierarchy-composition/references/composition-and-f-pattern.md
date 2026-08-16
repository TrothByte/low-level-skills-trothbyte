# Composition and F-Pattern

Rule format: RULE / WHY AI GETS IT WRONG / CORRECT REASONING / EXAMPLE /
COUNTEREXAMPLE / VERIFICATION / SOURCE. Facts marked **VERIFIED** were
observed on this host (Python 3.11.9) with the fixtures under `examples/`.

## 1. F-pattern: users scan the top and the left first

- **RULE**: eye-tracking research (NN/g) shows reading follows an F shape:
  a horizontal scan of the first line, a shorter second horizontal scan,
  then a vertical scan down the left edge. Put the thesis and primary CTA
  in that path.
- **WHY AI GETS IT WRONG**: the agent lays out the page in generation order
  (nav, then sections, then CTA) so the value proposition is right-aligned
  or below the fold — outside the scan path.
- **CORRECT REASONING**: placement is content strategy. The first line of
  body copy and the top-left region get the most attention; the middle and
  right edge get the least. Rank placement accordingly.
- **EXAMPLE**: h1 → thesis paragraph → CTA → first `h2` section, all in
  the top-left flow.
- **COUNTEREXAMPLE**: the thesis in paragraph five, right-aligned, below a
  large decorative block.
- **VERIFICATION**: INFERRED for eye-tracking (NN/g research, not
  reproduced here); the checker enforces the mechanical proxy: the first
  paragraph must appear before the middle of the document and share the
  page's key terms.
- **SOURCE**: https://www.nngroup.com/articles/f-shaped-pattern-reading-web-content/ (proposed `nngroup-f-pattern`)

## 2. Hero as thesis

- **RULE**: the hero region is the thesis statement: an `h1` that names the
  product/purpose, one paragraph that says what it does and why it matters,
  and a single primary CTA. No filler, no "Welcome".
- **WHY AI GETS IT WRONG**: generated heroes say "Welcome to our website"
  or "Transform your business" — a thesis-shaped hole with no content; the
  real message is buried below.
- **CORRECT REASONING**: the hero is the promise the page must deliver on.
  If the first paragraph does not share the page's key terms, the hero is
  decoration, not communication.
- **EXAMPLE**: h1 "Atlas railway analytics", p "Atlas turns raw train
  telemetry into delay predictions your control room can act on within
  seconds.", CTA "Start with the fleet-health dashboard".
- **COUNTEREXAMPLE**: h1 "Welcome", lorem p, CTA "Learn more".
- **VERIFICATION**: VERIFIED proxy — the checker requires a significant
  `<title>` keyword in the first paragraph ("thesis not in the hero" is
  reported when absent).
- **SOURCE**: https://www.nngroup.com/articles/visual-hierarchy-ux-definition/ (proposed `nngroup-visual-hierarchy`); https://claude.com/blog/improving-frontend-design-through-skills (proposed `claude-frontend-design-blog`)

## 3. The squint test as a manual verification method

- **RULE**: to judge hierarchy, squint or blur the rendered page until
  detail disappears; 3–4 distinct levels of visual weight should remain.
  If the page flattens to one gray mass, hierarchy is flat and must be
  rebuilt with contrast.
- **WHY AI GETS IT WRONG**: the agent evaluates hierarchy at full fidelity
  on a desktop screenshot, where every element "looks important".
- **CORRECT REASONING**: the squint test removes text-level detail and
  leaves only weight. It is the honest test of hierarchy; screenshots at
  100% zoom are the dishonest one.
- **EXAMPLE**: squinting the good page leaves: hero (large), sections
  (medium), body (small) — three levels.
- **COUNTEREXAMPLE**: squinting a generated landing page leaves one
  uniform gray field.
- **VERIFICATION**: UNVERIFIED as an automated check (human visual method);
  the static checker verifies the structural preconditions (single h1,
  order, thesis placement, weight contrast via the typography checker).
- **SOURCE**: https://www.nngroup.com/articles/visual-hierarchy-ux-definition/ (proposed `nngroup-visual-hierarchy`); https://github.com/anthropics/skills/blob/main/skills/frontend-design/SKILL.md (proposed `anthropic-frontend-design-skill`)
