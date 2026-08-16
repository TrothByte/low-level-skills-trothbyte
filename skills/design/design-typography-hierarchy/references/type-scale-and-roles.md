# Type Scale and Roles

Rule format: RULE / WHY AI GETS IT WRONG / CORRECT REASONING / EXAMPLE /
COUNTEREXAMPLE / VERIFICATION / SOURCE. Facts marked **VERIFIED** were
observed on this host (Python 3.11.9) with the fixtures under `examples/`.

## 1. Type sizes come from a deliberate scale, not per-element intuition

- **RULE**: choose a base size and a ratio (1.125 minor-second, 1.2 minor-
  third, 1.25 major-third, 1.333 perfect-fourth) and derive the ramp as
  base × ratio^n. Every used size must land on a step of that scale.
- **WHY AI GETS IT WRONG**: the agent picks each size to "look right" next
  to the mock (34, 23, 19, 15, 14, 12, 11 px), producing a ramp whose gaps
  match no scale and cannot be reasoned about.
- **CORRECT REASONING**: a scale makes every size computable and reviewable.
  A gap that matches no step (e.g. 23→34 px, ratio 1.478) is a defect you
  can point at. Fluid display sizes belong in `clamp()` so they respond to
  viewport width while staying intentional.
- **EXAMPLE**: base 16px, ratio 1.25 → 12.8, 16, 20, 25, 31.25 px.
- **COUNTEREXAMPLE**: 34px/23px/19px/15px/14px/12px/11px — a long arbitrary
  ramp.
- **VERIFICATION**: VERIFIED — `type_scale_check.py` reports the bad ramp:
  "7 distinct font sizes … not a deliberate scale" plus four gaps that
  "match no modular step" (exit 1); the good 1.25 scale passes (exit 0).
- **SOURCE**: https://www.modularscale.com/ (proposed `modular-scale`); https://utopia.fyi/calculator (proposed `utopia-fluid-type`); https://www.nngroup.com/articles/visual-hierarchy-ux-definition/ (proposed `nngroup-visual-hierarchy`)

## 2. Keep the ramp short — few sizes, many roles

- **RULE**: use a small number of sizes per screen (NN/g guidance: a handful
  at most; long ramps of 7+ distinct sizes are the documented AI-slop
  pattern). Assign roles (display/heading/body/caption) instead of adding
  sizes.
- **WHY AI GETS IT WRONG**: the agent adds a size for every new element
  ("statistic text", "footer text", "tag text") and the page ends up with a
  font-size for everything and hierarchy for nothing.
- **CORRECT REASONING**: hierarchy comes from contrast between a few steps,
  not from many steps. Fewer sizes = easier theming and fewer spacing bugs.
- **EXAMPLE**: display (fluid), h1–h3 from the scale, body 1rem, caption
  0.8rem — five roles, four steps.
- **COUNTEREXAMPLE**: seven raw sizes, each used once.
- **VERIFICATION**: VERIFIED — `type_scale_check.py` flags ≥ 7 distinct
  sizes; the good fixture has 5 (12.8/16/20/25/31.25 px) and passes.
  Confidence on the exact "≤ 3" number: INFERRED heuristic (NN/g style
  guidance); the checker uses 7 as a firm smell threshold.
- **SOURCE**: https://www.nngroup.com/articles/visual-hierarchy-ux-definition/ (proposed `nngroup-visual-hierarchy`); https://claude.com/blog/improving-frontend-design-through-skills (proposed `claude-frontend-design-blog`)

## 3. Weight contrast, not size alone, carries hierarchy

- **RULE**: headings should use a visibly heavier weight than body (e.g.
  600/700 vs 400). Avoid all-light ramps (100/200/300 only) and avoid
  headings that equal body weight with close sizes.
- **WHY AI GETS IT WRONG**: agents produce "premium-feeling" pages with
  light-weight headings (100/200) and no heavy counterpart, or set every
  heading to the body weight and rely only on size.
- **CORRECT REASONING**: a light-only ramp fails at any screen scale — sizes
  compress, hierarchy disappears. Contrast of weight gives hierarchy even
  when sizes must shrink (small viewports, truncation).
- **EXAMPLE**: body 400, strong 600, headings 700.
- **COUNTEREXAMPLE**: `h1{font-weight:100} h2{200} h3{300}` and nothing ≥ 700.
- **VERIFICATION**: VERIFIED — `type_scale_check.py` reports "all heading
  weights are light (≤ 300) with no 700+ anywhere" for the bad fixture.
- **SOURCE**: https://www.nngroup.com/articles/visual-hierarchy-ux-definition/ (proposed `nngroup-visual-hierarchy`); https://github.com/anthropics/skills/blob/main/skills/frontend-design/SKILL.md (proposed `anthropic-frontend-design-skill`)

## 4. No default font stacks — pick a brand family and lead with it

- **RULE**: the first family in `font-family` must be a deliberate brand or
  custom family; Inter, Roboto, system-ui, -apple-system, "Segoe UI" are
  default stacks that both weaken the design and fingerprint it as
  AI-generated.
- **WHY AI GETS IT WRONG**: "System Font Stack" is a one-line comfort default
  that models output heavily; agents reach for it on every project.
- **CORRECT REASONING**: typeface choice is a brand decision. A custom family
  (self-hosted or from the brand guide) is the default; generic fallbacks are
  the tail of the stack only.
- **EXAMPLE**: `font-family: "Spline Sans", sans-serif;` (brand leads).
- **COUNTEREXAMPLE**: `font-family: Inter, -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;`.
- **VERIFICATION**: VERIFIED — `font_stack_check.py` flags the Inter-led
  stack (exit 1) and accepts the brand-led stack (exit 0).
- **SOURCE**: https://claude.com/blog/improving-frontend-design-through-skills (proposed `claude-frontend-design-blog`); https://github.com/anthropics/skills/blob/main/skills/frontend-design/SKILL.md (proposed `anthropic-frontend-design-skill`)
