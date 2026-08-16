# Design Token Hierarchy — Primitive vs Semantic Layers

Rule format: RULE / WHY AI GETS IT WRONG / CORRECT REASONING / EXAMPLE /
COUNTEREXAMPLE / VERIFICATION / SOURCE. Facts marked **VERIFIED** were
observed on this host (Python 3.11.9, Node v26.4.0, style-dictionary 5.5.1)
with the fixtures under `examples/`.

## 1. Two layers: primitives and semantic tokens

- **RULE**: split the token system into (a) primitives — raw brand values
  (exact colors, spacing units, font families) — and (b) semantic tokens —
  meaning-bound aliases (`color.text.primary`, `space.card`). Components may
  only consume semantic tokens; primitives exist only to be aliased.
- **WHY AI GETS IT WRONG**: agents flatten one list of names and values and
  let components reference whichever name is convenient; after the first
  theme change, "text.primary" and the component's direct hex diverge.
- **CORRECT REASONING**: the semantic layer is the stable contract between
  the design system and the product. Rebranding edits primitives only; the
  semantic layer and all components keep working because they never held
  raw values. Components that bypass semantic tokens are the leak.
- **EXAMPLE**: semantic `color.action = {color.brand.500}`; the button uses
  `var(--semantic-color-action)`; swapping the brand color changes one
  primitive and nothing else.
- **COUNTEREXAMPLE**: `semantic.color.text.primary = "#1F2937"` (raw literal
  in the semantic layer) and `.button { background: #4F46E5 }` — two copies
  of the brand color already exist.
- **VERIFICATION**: VERIFIED — `verify_dtcg.py` rejects `examples/bad/
  tokens.json` with "semantic token must alias a primitive" for the raw
  semantic literal, and `verify_no_raw_literals.py` flags all 6 raw values
  in `examples/bad/component.css` (exit 1). The good pair passes both
  checks (exit 0).
- **SOURCE**: https://tr.designtokens.org/format/ (DTCG, proposed `dtcg-design-tokens`); https://github.com/anthropics/skills/blob/main/skills/frontend-design/SKILL.md (proposed `anthropic-frontend-design-skill`)

## 2. Generated CSS, never hand-duplicated

- **RULE**: the emitted `:root { --x: y }` block is a build artifact derived
  from the token file (style-dictionary or equivalent). Do not hand-write
  the CSS variables — that recreates a second source of truth.
- **WHY AI GETS IT WRONG**: the agent "helps" by pasting a nice `:root` block
  into the app so the tokens appear wired up; the next change lands in only
  one of the two files and the system silently breaks.
- **CORRECT REASONING**: the build makes every output (CSS, JS theme objects,
  native tokens) derive from one file. "The tokens and the CSS agree" is
  therefore a property of the pipeline, not a review promise.
- **EXAMPLE**: `npx style-dictionary build` consumes `tokens.json` and emits
  `tokens.css`; component CSS references `var(--semantic-color-action)`.
- **COUNTEREXAMPLE**: a checked-in `:root` block whose values were typed by
  hand while `tokens.json` also lives in the repo.
- **VERIFICATION**: VERIFIED — `npx --yes style-dictionary@5.5.1 build
  --config examples/sd.config.json` (Node v26.4.0) produced
  `examples/good/build/tokens.css` with 21 derived variables, including
  `--semantic-color-action: #4f46e5` resolved through the alias chain.
- **SOURCE**: https://tr.designtokens.org/format/ (proposed `dtcg-design-tokens`); https://claude.com/blog/improving-frontend-design-through-skills (proposed `claude-frontend-design-blog`)

## 3. No raw literals in component CSS

- **RULE**: a component stylesheet must contain no `#hex`, `rgb()`, `hsl()`,
  or unit-length literals for values the design system owns (colors, spacing,
  radii, type sizes). All such values arrive as `var(--...)`.
- **WHY AI GETS IT WRONG**: the agent sees a hex in the design mock, writes
  it directly into the component ("it's just one color"), and the result
  looks right while being unthemeable and ungovernable.
- **CORRECT REASONING**: a raw literal in a component is a real, greppable
  defect. Scanning for it is deterministic; fixing it means adding a semantic
  token and aliasing the primitive.
- **EXAMPLE**: `.button { background-color: var(--semantic-color-action); }`.
- **COUNTEREXAMPLE**: `.button { background-color: #4F46E5; padding: 8px 16px; }`.
- **VERIFICATION**: VERIFIED — `verify_no_raw_literals.py` reports 6 raw
  literals in `examples/bad/component.css` (exit 1) and 0 in the good file
  (exit 0).
- **SOURCE**: https://github.com/anthropics/skills/blob/main/skills/frontend-design/SKILL.md (proposed `anthropic-frontend-design-skill`); https://arxiv.org/abs/2403.03163 (proposed `arxiv-2403-03163`)
