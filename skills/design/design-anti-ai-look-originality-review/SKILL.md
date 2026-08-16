---
name: design-anti-ai-look-originality-review
description: Use when reviewing generated UI for generic AI styling or when asked to make a design feel original. Teaches fingerprinting AI-look markers, a uniqueness self-test, brand grounding, and spending boldness in one place.
---

# Design Anti-AI-Look & Originality Review

## When to use

- Reviewing generated UI that "looks like AI" but nobody can say why.
- Making a design feel original: brand grounding, custom palette/type, one bold idea.
- Auditing a page against the documented AI-slop marker list (default fonts,
  purple gradients, cream/terracotta, acid-green, hairlines, 01/02/03, template hero).
- Writing a design brief for an agent that must NOT produce the default template.
- Judging uniqueness honestly instead of "I've seen this before" vibes.

## When not to use

- Mechanical contrast/structure/accessibility — `design-color-contrast-wcag-a11y`,
  `design-visual-hierarchy-composition`.
- Token systems that enable custom palettes — `design-token-system-discipline`.
- Type scale decisions (Space Grotesk is a fingerprint, but the scale itself
  is `design-typography-hierarchy`).
- Claims of legal/trademark-level originality — this is a design-process
  self-test, not a legal opinion.

## What the agent often gets wrong

- Defaults to the canonical AI template: h1 + subhead + gradient CTA + three
  cards, numbered 01/02/03, Inter or Space Grotesk, hairline dividers.
- Uses Inter/Roboto/system stacks on every project, which is the strongest
  single fingerprint of generated UI.
- Ships "purple gradient on white" as the default brand treatment (and the
  flat variant — purple text/accents on white).
- Mimics the last fashionable style (cream + serif + terracotta, near-black
  + acid-green) without a reason to.
- Confuses "more styling" with "more originality" — adds gradients, shadows,
  and gradients-of-gradients, producing MORE AI-look, not less.
- Self-evaluates originality by recognition ("looks unique to me") instead
  of running the fingerprint scan and the brand-grounding check.

## How to reason correctly

1. Run the fingerprint scanner: 8 documented AI-look families (default font
   stacks, purple gradients on white, cream+serif+terracotta, near-black+
   acid-green, broadsheet hairlines, Space Grotesk drift, numbered labels,
   template hero). Score ≥ 3 means the design is defaulting.
2. Ground the design in the brand: use the brand's actual colors (not the
   generic AI palette), a real custom typeface or a deliberate system, and
   make the brand name appear in the copy.
3. Spend your boldness in one place: pick a single distinctive idea (a
   color, a type move, a layout gesture) and commit to it; everything else
   stays quiet. One loud choice is original; ten loud choices is noise.
4. Prefer substance: original copy, real product specifics, and a
   distinctive structure beat decorative gradients every time.
5. Verify with the uniqueness self-test and the fingerprint scan — not by
   "feel".

## What to verify

- Fewer than 3 AI-look fingerprint families match.
- No default font stack (Inter/Roboto/system) leads a `font-family`.
- The palette contains ≥ 2 colors outside the generic AI palette.
- The brand name appears in body copy.
- Exactly one bold idea carries the design (single CTA, single accent).
- The page is not the h1+subhead+gradient-CTA+3-cards template.

## How to verify

```
python examples/ai_look_fingerprint.py examples/good/index.html examples/good/style.css
python examples/ai_look_fingerprint.py examples/bad/index.html examples/bad/style.css   # exit 1: 6 fingerprints
python examples/uniqueness_self_test.py examples/good/index.html examples/good/style.css
python examples/uniqueness_self_test.py examples/bad/index.html examples/bad/style.css  # exit 1: GENERIC
```

Verified on this host (Python 3.11.9): the good Halcyon page scores 0
fingerprints and passes the uniqueness self-test (brand in copy, 4 custom
colors, Spline Sans leads); the bad page scores 6 fingerprints (Inter,
purple gradient, hairline, Space Grotesk, 01/02/03, template hero) and is
rejected as GENERIC. Full outputs in `evals/README.md`.

## Where the knowledge comes from

- Anthropic — "Improving frontend design through skills" (AI-look markers and original-design guidance): https://claude.com/blog/improving-frontend-design-through-skills (proposed source id `claude-frontend-design-blog`)
- Anthropic frontend design skills — the documented "AI-slop" marker set: https://github.com/anthropics/skills/blob/main/skills/frontend-design/SKILL.md (proposed source id `anthropic-frontend-design-skill`)
- NN/g visual hierarchy — why a single focal point beats uniform decoration: https://www.nngroup.com/articles/visual-hierarchy-ux-definition/ (proposed source id `nngroup-visual-hierarchy`)
- Design2Code (arxiv 2403.03163) — generated UIs collapse toward generic templates: https://arxiv.org/abs/2403.03163 (proposed source id `arxiv-2403-03163`)

## Related skills

- `design-token-system-discipline` (recommend) — brand palette lives in tokens
- `design-typography-hierarchy` (recommend) — Space Grotesk drift is a fingerprint; custom type is brand grounding
- `design-color-contrast-wcag-a11y` (recommend) — replacing generic palettes must keep contrast
- `design-layout-spacing-grid` (recommend) — hairline dividers get replaced by spacing discipline
- `design-visual-hierarchy-composition` (recommend) — the "one bold idea" is a hierarchy decision
- `meta-verification-harness-validity` (recommend) — the fingerprint scanner is the harness; it must catch real AI-look
- `meta-verification` (recommend) — "original" needs a test, not a feeling

## Evaluation

Synthetic: each of the 8 fingerprint families individually (Inter stack,
purple gradient, cream+serif+terracotta, near-black+acid-green, hairlines,
Space Grotesk, 01/02/03, template hero); threshold at ≥ 3 families.

False-positive: a brand that genuinely uses one listed element deliberately
(a fintech with a violet accent, a news brand with hairlines) — the
fingerprint must be judged as a system, not per-element; a single custom
accent that is loud on purpose.

Historical: the template-hero regression — a generated landing page matched
all 8 markers and read as instantly generic; the redesign kept one brand
color and a distinctive headline structure. Design2Code documents
template-collapse in generated UI.

Adversarial: "violet is our brand color" — agent must check the rest of the
system (is the whole identity built on the generic palette?); "add more
gradients to make it pop" — agent must reject decoration-as-originality;
"it's fine, I've seen worse" — agent must run the scanner and the
uniqueness test; "the client asked for cream and terracotta" — agent must
apply the deliberate exception rule and document it.

## Notes on claim confidence

The fingerprint families are KNOWN (documented AI-slop marker set from the
Claude frontend-design blog and Anthropic frontend-design skill). The
"spend your boldness in one place" principle is INFERRED design guidance
(same sources). The scanner threshold (≥ 3 families = probable AI-look) is
an INFERRED calibration; per-element exceptions are deliberate when the
rest of the system is grounded.
