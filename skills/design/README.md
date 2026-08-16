# design — Skills

Designer-mode skills for AI agents: design tokens (DTCG), typography hierarchy, WCAG 2.2 color/contrast accessibility, layout grids and reflow, visual hierarchy, and anti-AI-look originality review.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `design-anti-ai-look-originality-review` | Use when reviewing generated UI for generic AI styling or when asked to make a design feel original. Teaches fingerprinting AI-look markers, a uniqueness self-test, brand grounding, and spending boldness in one place. | unique | source-backed | `skills/design/design-anti-ai-look-originality-review` |
| `design-color-contrast-wcag-a11y` | Use when choosing color pairs or verifying accessibility of text, UI, and images. Teaches WCAG 2.2 as a design contract: relative luminance and contrast math, 4.5:1/3:1 thresholds, alt/labels/lang, 24px targets, axe-core/Lighthouse verification. | improved | source-backed | `skills/design/design-color-contrast-wcag-a11y` |
| `design-layout-spacing-grid` | Use when building or reviewing page layout, spacing, and responsive grids. Teaches a 4/8pt spacing scale, 12-column grids with consistent gutters, WCAG 1.4.10 reflow at 320px, and eliminating arbitrary px margins. | improved | source-backed | `skills/design/design-layout-spacing-grid` |
| `design-token-system-discipline` | Use when building or reviewing design tokens, theme files, or component CSS that hard-codes colors or spacing. Teaches DTCG format ($type/$value, aliases, composite tokens), semantic vs primitive layers, and token-first CSS without raw hex or px literals. | improved | source-backed | `skills/design/design-token-system-discipline` |
| `design-typography-hierarchy` | Use when choosing type sizes, weights, or font stacks, or when reviewing heading structure. Teaches modular type scales and clamp(), display vs body roles, weight contrast, ≤3 sizes, WCAG text spacing, and never skipping heading levels. | improved | source-backed | `skills/design/design-typography-hierarchy` |
| `design-visual-hierarchy-composition` | Use when structuring a page or evaluating hierarchy and composition. Teaches NN/g visual hierarchy: exactly one h1, heading order, squint test, F-pattern, hero-as-thesis, and key content in the first two paragraphs. | common | source-backed | `skills/design/design-visual-hierarchy-composition` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
  (`references/` hold deep knowledge; `examples/good` and `examples/bad` are compiled/run
  fixtures; `evals/README.md` defines eval cases.)
- Load only the skill you need (see `skills/_meta/meta-routing`; references load on demand.

## Related

[Back to repository root](../../README.md)
