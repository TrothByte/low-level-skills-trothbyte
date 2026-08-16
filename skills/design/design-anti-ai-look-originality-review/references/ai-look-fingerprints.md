# AI-Look Fingerprints

Rule format: RULE / WHY AI GETS IT WRONG / CORRECT REASONING / EXAMPLE /
COUNTEREXAMPLE / VERIFICATION / SOURCE. Facts marked **VERIFIED** were
observed on this host (Python 3.11.9) with the fixtures under `examples/`.

## 1. Default font stacks are the strongest single fingerprint

- **RULE**: never let Inter, Roboto, system-ui, -apple-system, or "Segoe UI"
  lead a `font-family`. A default stack is both a design decision made by
  nobody and the #1 marker that a page was generated.
- **WHY AI GETS IT WRONG**: "System Font Stack" is the one-line comfort
  default; models reach for it on every project, so it correlates with
  generated output at scale.
- **CORRECT REASONING**: typeface is a brand decision. A custom or
  brand-specified family leads; defaults are generic fallbacks at the tail.
  If the design has no custom type, the design is unfinished.
- **EXAMPLE**: `font-family: "Spline Sans", sans-serif;`.
- **COUNTEREXAMPLE**: `font-family: Inter, -apple-system, system-ui, sans-serif;`.
- **VERIFICATION**: VERIFIED — `ai_look_fingerprint.py` flags the Inter-led
  stack (line 11 of the bad CSS) and scores 0 on the good page.
- **SOURCE**: https://claude.com/blog/improving-frontend-design-through-skills (proposed `claude-frontend-design-blog`); https://github.com/anthropics/skills/blob/main/skills/frontend-design/SKILL.md (proposed `anthropic-frontend-design-skill`)

## 2. Purple gradients on white and the other color clichés

- **RULE**: detect and avoid the documented color-system clichés: purple/
  violet gradients on white (#7C3AED/#8B5CF6/#6366F1), cream + serif +
  terracotta (#F4F1EA/#E07A5F), near-black + acid-green (#0F172A/#A3E635).
  They are not "brand palettes"; they are templates.
- **WHY AI GETS IT WRONG**: the model was trained on designs that used these
  exact palettes, so they appear with high probability in generation and
  read as instantly generic to anyone who has seen ten generated sites.
- **CORRECT REASONING**: a brand palette is specific to the brand. "The
  palette is 2 custom colors + neutrals, not the generic AI set" is a
  checkable property (see the uniqueness test). Reuse of a template palette
  is reuse, not design.
- **EXAMPLE**: Halcyon's `--ink: #0F3D2E` + `--amber: #E8A33D` on warm
  white — not in the generic set.
- **COUNTEREXAMPLE**: `.hero { background: linear-gradient(135deg, #7C3AED, #8B5CF6); }` on white.
- **VERIFICATION**: VERIFIED — the scanner found "purple/violet gradient"
  (lines 43/48) and the uniqueness test reported "colors are all from the
  generic AI palette" for the bad fixture.
- **SOURCE**: https://claude.com/blog/improving-frontend-design-through-skills (proposed `claude-frontend-design-blog`); https://github.com/anthropics/skills/blob/main/skills/frontend-design/SKILL.md (proposed `anthropic-frontend-design-skill`)

## 3. Structural tells: hairlines, Space Grotesk, 01/02/03, template hero

- **RULE**: the layout-level markers are also fingerprints: broadsheet
  hairline dividers (`1px solid #E5E7EB` section rules), Space Grotesk
  heading drift, numbered 01/02/03 labels, and the canonical hero of h1 +
  subhead + gradient CTA + three cards. Match ≥ 3 families and the page
  reads as AI-generated.
- **WHY AI GETS IT WRONG**: each marker individually is "a valid design
  choice", so the agent defends them one at a time while the combination
  screams template.
- **CORRECT REASONING**: judge the system, not the element. Replace
  hairlines with spacing, replace numbered lists with real hierarchy, and
  replace the template hero with the actual thesis. One deliberate marker
  can survive as an exception only when the rest is grounded.
- **EXAMPLE**: a page with zero of these structural tells (spacing-led
  dividers, no numbered labels, thesis hero).
- **COUNTEREXAMPLE**: Inter + Space Grotesk + purple gradient + hairlines +
  01/02/03 + template hero = score 6.
- **VERIFICATION**: VERIFIED — `ai_look_fingerprint.py` scored the bad
  fixture at 6 distinct families (threshold ≥ 3 ⇒ exit 1) and the good
  fixture at 0 (exit 0).
- **SOURCE**: https://claude.com/blog/improving-frontend-design-through-skills (proposed `claude-frontend-design-blog`); https://arxiv.org/abs/2403.03163 (proposed `arxiv-2403-03163`)

## 4. The fingerprint threshold is a calibration, not a law

- **RULE**: treat the ≥ 3-family threshold as the strong signal; 1–2
  families are a review prompt, not a verdict. A brand may deliberately own
  one marker (e.g. a violet accent) when the rest of the system is grounded.
- **WHY AI GETS IT WRONG**: agents flip between "one violet is fine" and
  "any gradient is bad" with no systematic rule.
- **CORRECT REASONING**: originality is a system property: custom palette +
  custom type + real copy + one bold idea. Deliberate use of one cliché
  with everything else grounded is different from defaulting to the clichés.
- **EXAMPLE**: a brand whose single accent is violet but whose type,
  palette, copy, and layout are bespoke — deliberate exception, documented.
- **COUNTEREXAMPLE**: a page that is violet-on-white AND Inter AND hairlines.
- **VERIFICATION**: INFERRED calibration (documented in `evals/README.md`);
  the scanner exits 0 below the threshold and 1 at or above it.
- **SOURCE**: https://claude.com/blog/improving-frontend-design-through-skills (proposed `claude-frontend-design-blog`); https://www.nngroup.com/articles/visual-hierarchy-ux-definition/ (proposed `nngroup-visual-hierarchy`)
