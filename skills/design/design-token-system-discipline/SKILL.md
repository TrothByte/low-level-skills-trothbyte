---
name: design-token-system-discipline
description: Use when building or reviewing design tokens, theme files, or component CSS that hard-codes colors or spacing. Teaches DTCG format ($type/$value, aliases, composite tokens), semantic vs primitive layers, and token-first CSS without raw hex or px literals.
---

# Design Token System Discipline

## When to use

- Creating or editing a design-token file (`tokens.json`, `tokens.css`, theme files).
- Reviewing component CSS that hard-codes `#hex` colors, `px` sizes, or spacing.
- Implementing a theming layer or migrating hard-coded styles to a token system.
- Setting up a style-dictionary-style build so one token file feeds CSS/JS/native.
- Verifying that aliases, composite tokens, and semantic tokens resolve correctly.

## When not to use

- One-off prototype or throwaway HTML where no theme or reuse exists — still avoid
  inventing values the design has already defined, but a full DTCG pipeline is overkill.
- Verifying WCAG contrast of a specific color pair — use
  `design-color-contrast-wcag-a11y`; tokens alone do not guarantee contrast.
- Choosing type sizes or line heights — that is `design-typography-hierarchy`.
- External brand guidelines work: token naming should mirror the brand vocabulary,
  but this skill is about the format and the discipline, not the names themselves.

## What the agent often gets wrong

- Hard-codes the resolved color/spacing in the component instead of emitting
  `var(--semantic-...)` — the component then silently diverges from the theme.
- Puts raw values in the semantic layer ("semantic.color.primary = #1F2937")
  instead of aliasing a primitive, so the semantic layer is not a stable
  contract and theming breaks.
- Writes aliases that point at tokens that do not exist (`{color.brand.999}`);
  the build fails late or, worse, silently keeps stale hard-coded fallbacks.
- Forgets `$type` or `$value` on tokens; AI-slop token files that look right
  but are not valid DTCG and cannot be consumed by style-dictionary.
- Duplicates the same hex in multiple token files ("design-system
  non-compliance"): two sources of truth drift apart on the first theme change.
- Skips the build step entirely and hand-writes `:root {}` CSS variables that
  repeat the values — the tokens.json stops being the single source of truth.

## How to reason correctly

1. Split the token set into two layers: primitives (raw brand values — exact
   colors, spacing units, font families) and semantic tokens (meaning-bound
   aliases: `text.primary`, `space.card`, `color.action`).
2. Write every token in DTCG format: each non-group node carries `$type` and
   `$value`; semantic tokens reference primitives with `{path.to.token}`.
3. Treat semantic tokens as the only vocabulary components may use; a component
   CSS file that contains a hex code or a px length is already a defect.
4. Generate the CSS custom properties from the token file with a build step
   (style-dictionary), never by hand, so the emitted `:root` block is derived,
   not duplicated.
5. Verify: the token file validates, aliases resolve, the build produces CSS,
   and a raw-literal scan of component CSS returns zero matches.

## What to verify

- Every token has `$type` and `$value`; `$type` values are in the DTCG registry.
- Every `{alias}` resolves to an existing token path (no dangling references).
- Semantic-layer tokens contain no raw literals — only aliases to primitives.
- Composite tokens (typography, border, shadow) carry an object `$value` whose
  parts resolve.
- Component CSS contains zero raw `#hex`/`rgb()`/`px` literals — only `var(--...)`.
- The build step (style-dictionary) succeeds and emits the expected `:root`
  block with both primitive and semantic variables.

## How to verify

```
python examples/verify_dtcg.py examples/good/tokens.json
python examples/verify_dtcg.py examples/bad/tokens.json      # exit 1: 3 issues
python examples/verify_no_raw_literals.py examples/good/component.css
python examples/verify_no_raw_literals.py examples/bad/component.css   # 6 raw literals
npx --yes style-dictionary@5.5.1 build --config examples/sd.config.json
```

Verified on this host (Python 3.11.9, Node v26.4.0): the good token file
reports 21 tokens with all aliases resolving; the bad file is rejected with
3 issues; the good component CSS is clean; the bad CSS has 6 raw literals;
style-dictionary 5.5.1 builds `examples/good/build/tokens.css` including the
composite typography token. Full outputs in `evals/README.md`.

## Where the knowledge comes from

- DTCG Design Tokens Community Group — format specification: https://tr.designtokens.org/format/ (proposed source id `dtcg-design-tokens`)
- Anthropic frontend design skills — token-driven styling practice: https://github.com/anthropics/skills/blob/main/skills/frontend-design/SKILL.md (proposed source id `anthropic-frontend-design-skill`)
- Anthropic — "Improving frontend design through skills" (design tokens as first-class tooling): https://claude.com/blog/improving-frontend-design-through-skills (proposed source id `claude-frontend-design-blog`)
- Design2Code (arxiv 2403.03163) — model output diverges from visual spec without disciplined token/constraint adherence (proposed source id `arxiv-2403-03163`)

## Related skills

- `design-typography-hierarchy` (recommend) — type tokens must be consumed through the same token discipline
- `design-color-contrast-wcag-a11y` (recommend) — semantic color tokens must still pass contrast ratios
- `design-layout-spacing-grid` (recommend) — spacing tokens replace arbitrary px margins
- `design-anti-ai-look-originality-review` (recommend) — token systems are the hook for brand-specific originality
- `meta-verification-harness-validity` (recommend) — the verify scripts here are the harness; make sure they fail on real defects
- `meta-verification` (recommend) — evidence over vibes when claiming "the theme is consistent"
- `performance-measurement-discipline` (recommend) — token builds are derived artifacts, not measured workloads

## Evaluation

Synthetic: valid vs invalid DTCG files (missing `$type`, dangling alias,
raw literal in semantic layer); alias chain resolution; composite typography
token validation; raw-literal scan of component CSS.

False-positive: a file with legitimately local values (a demo page that is
not part of the token system, `@font-face` src descriptors, media-query
breakpoints) must not be flagged.

Historical: design-system drift incidents — a component whose hex diverged
from the token after a rebrand (known real-world failure class); DTCG
format spec updates.

Adversarial: "this hex is the brand color, just inline it once" — agent must
route it through the semantic layer; "tokens.json failed to build, delete
the $type fields" — agent must fix the format, not strip it; "just add a
hard-coded fallback color next to the var()" — agent must reject duplicated
values.
