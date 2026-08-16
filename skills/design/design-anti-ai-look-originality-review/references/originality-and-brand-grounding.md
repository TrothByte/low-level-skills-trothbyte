# Originality and Brand Grounding

Rule format: RULE / WHY AI GETS IT WRONG / CORRECT REASONING / EXAMPLE /
COUNTEREXAMPLE / VERIFICATION / SOURCE. Facts marked **VERIFIED** were
observed on this host (Python 3.11.9) with the fixtures under `examples/`.

## 1. Brand grounding beats decoration

- **RULE**: originality comes from grounding in the brand — its actual
  colors, its typeface, its product specifics — not from adding decorative
  flourishes. The brand name must appear in the copy and the palette/type
  must be the brand's, not the template's.
- **WHY AI GETS IT WRONG**: the agent interprets "make it unique" as "add
  more gradients and effects", producing MORE AI-look while the content
  stays generic filler.
- **CORRECT REASONING**: a visitor cannot tell that two brands share a
  purple gradient, but they can tell when a page talks about its actual
  product. Substance is the originality; decoration is optional.
- **EXAMPLE**: Halcyon — "Halcyon moves freight on schedules shippers can
  actually trust…" with a deadline-backed-schedules card.
- **COUNTEREXAMPLE**: "Revolutionize Your Workflow / Unlock the power of
  intelligent automation for modern teams."
- **VERIFICATION**: VERIFIED proxy — `uniqueness_self_test.py` checks the
  brand name (title first word) appears in body copy; the good fixture
  passes, the bad fixture fails on palette/type/fingerprint checks.
- **SOURCE**: https://claude.com/blog/improving-frontend-design-through-skills (proposed `claude-frontend-design-blog`); https://github.com/anthropics/skills/blob/main/skills/frontend-design/SKILL.md (proposed `anthropic-frontend-design-skill`)

## 2. The palette and type must be custom, not the generic AI set

- **RULE**: a design is grounded when ≥ 2 of its colors are outside the
  generic AI palette and its leading font is not a default stack. These are
  the two cheap, checkable proxies for "custom".
- **WHY AI GETS IT WRONG**: the model reproduces the palettes it saw most
  (indigo/violet, cream/terracotta, dark+acid), so "the colors look brand
  new" to the model while being the most common on the internet.
- **CORRECT REASONING**: custom = not in the high-frequency set. Checking
  set membership is deterministic; a palette of 4 template colors has no
  brand information in it.
- **EXAMPLE**: `#0F3D2E`, `#E8A33D`, `#FAF7F2`, `#24342C` (4 custom colors)
  + "Spline Sans"/"Fraunces" leading stacks.
- **COUNTEREXAMPLE**: `#7C3AED`, `#8B5CF6`, `#6366F1`, `#111827`, `#FFFFFF`
  + Inter/Space Grotesk.
- **VERIFICATION**: VERIFIED — the uniqueness test reported "4 colors
  outside the generic AI palette" and "'spline sans' leads the stack" for
  the good fixture, and FAIL for the bad fixture.
- **SOURCE**: https://claude.com/blog/improving-frontend-design-through-skills (proposed `claude-frontend-design-blog`); https://github.com/anthropics/skills/blob/main/skills/frontend-design/SKILL.md (proposed `anthropic-frontend-design-skill`)

## 3. Spend your boldness in one place

- **RULE**: choose exactly one bold idea — a color, a type move, a layout
  gesture, a headline structure — and commit to it; keep everything else
  quiet. One loud choice is distinctive; ten loud choices is noise, and
  noise is indistinguishable from AI output.
- **WHY AI GETS IT WRONG**: generated pages apply every effect to every
  element (gradients on all cards, shadows everywhere, multiple loud colors),
  achieving the maximum "designed" look and minimum distinctiveness.
- **CORRECT REASONING**: visual hierarchy (see
  `design-visual-hierarchy-composition`) requires most elements to be quiet
  so the loud one reads as the point. The boldness budget is spent once.
- **EXAMPLE**: Halcyon's one bold move is the amber CTA block against deep
  green on warm white; everything else is restrained.
- **COUNTEREXAMPLE**: every card with its own gradient, three accent colors,
  and a gradient hero.
- **VERIFICATION**: INFERRED (design guidance); the fingerprint scanner
  operationalizes it — decorating in the template style raises the score,
  restraint keeps it at 0.
- **SOURCE**: https://www.nngroup.com/articles/visual-hierarchy-ux-definition/ (proposed `nngroup-visual-hierarchy`); https://claude.com/blog/improving-frontend-design-through-skills (proposed `claude-frontend-design-blog`)
