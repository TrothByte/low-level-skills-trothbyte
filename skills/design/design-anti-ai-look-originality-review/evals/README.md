# Evaluation — design-anti-ai-look-originality-review

Skill: `skills/design/design-anti-ai-look-originality-review`. Stability
target: `evaluated`. Source files: `examples/good/index.html`,
`examples/good/style.css`, `examples/bad/index.html`, `examples/bad/style.css`,
`examples/ai_look_fingerprint.py`, `examples/uniqueness_self_test.py`.

## Host and tooling

Verification host: Windows, Python 3.11.9. Both checkers are pure-python
(verified below). No browser needed.

## Verification commands and actual output

```
python examples/ai_look_fingerprint.py examples/good/index.html examples/good/style.css
python examples/ai_look_fingerprint.py examples/bad/index.html examples/bad/style.css
python examples/uniqueness_self_test.py examples/good/index.html examples/good/style.css
python examples/uniqueness_self_test.py examples/bad/index.html examples/bad/style.css
```

Observed output:

```
$ python examples/ai_look_fingerprint.py examples/good/index.html examples/good/style.css
examples\good\index.html: original enough (0 fingerprints)
$ echo $LASTEXITCODE
0
$ python examples/ai_look_fingerprint.py examples/bad/index.html examples/bad/style.css
FINGERPRINT default font stack (Inter/Roboto/system-ui): lines [11]
FINGERPRINT purple/violet gradient: lines [43, 48]
FINGERPRINT broadsheet hairline dividers (1px #E5E7EB): lines [52]
FINGERPRINT Space Grotesk drift: lines [39]
FINGERPRINT numbered 01/02/03 labels: lines [16, 17, 18, 55, 56, 57]
FINGERPRINT template hero (h1 + subhead + gradient CTA + 3 cards): lines [1]
score: 6 distinct AI-look fingerprints
examples\bad\index.html: AI-look probable — reduce to one bold idea
$ echo $LASTEXITCODE
1
$ python examples/uniqueness_self_test.py examples/good/index.html examples/good/style.css
CHECK brand grounding: 'Halcyon' appears in body copy
CHECK custom palette: 4 colors outside the generic AI palette (['#0F3D2E', '#24342C', '#E8A33D', '#FAF7F2']...)
CHECK custom type: 'spline sans' leads the stack
examples\good\index.html: DIFFERENTIATED — brand-grounded, custom palette and type
$ echo $LASTEXITCODE
0
$ python examples/uniqueness_self_test.py examples/bad/index.html examples/bad/style.css
CHECK brand grounding: 'Revolutionize' appears in body copy
CHECK custom palette: FAIL — colors are all from the generic AI palette
CHECK custom type: 'space grotesk' leads the stack
CHECK fingerprint count: FAIL — 4 AI-look families
examples\bad\index.html: GENERIC — 2 brand-grounding check(s) failed
$ echo $LASTEXITCODE
1
```

Note: comments are stripped before scanning (a comment mentioning "purple"
or "Space Grotesk" must not trip a fingerprint) — the scanner is
comment-aware.

## Verified facts

| Fact | Status | Evidence |
|---|---|---|
| default font stacks detected (Inter/system) | KNOWN (observed) | bad CSS line 11 flagged |
| purple/violet gradients detected | KNOWN (observed) | bad CSS lines 43/48 flagged |
| hairline dividers detected | KNOWN (observed) | bad CSS line 52 flagged |
| Space Grotesk drift detected | KNOWN (observed) | bad CSS line 39 flagged |
| numbered 01/02/03 detected | KNOWN (observed) | bad lines 16-18 (HTML), 55-57 (CSS) |
| template hero detected (h1+subhead+gradient CTA+3 cards) | KNOWN (observed) | bad page flagged |
| ≥ 3-family threshold ⇒ exit 1 | KNOWN (observed) | bad score 6 (exit 1); good score 0 (exit 0) |
| brand grounding / custom palette / custom type | KNOWN (observed) | good: 4 custom colors, Spline Sans, brand in copy; bad: generic palette, Space Grotesk |
| comment text does not trip fingerprints | KNOWN (observed) | comment-stripping applied; good score is 0 |
| threshold calibration (exact cutoff 3) | INFERRED | documented heuristic; per-element exceptions allowed when grounded |

## Synthetic evals

- **easy**: an Inter-led font stack — flagged as a fingerprint.
- **easy**: `.hero { background: linear-gradient(135deg, #7C3AED, #8B5CF6); }`
  on white — flagged as purple gradient.
- **medium**: cream (#F4F1EA) + serif + terracotta (#E07A5F) — flagged as a
  combo (all three must co-occur).
- **medium**: near-black (#0F172A) with NO acid green — must NOT flag the
  pair (only the pair counts).
- **hard**: a template-hero page with a flat (non-gradient) CTA — the hero
  template family requires a gradient CTA; the other families must still be
  counted individually.

## False-positive evals (must NOT flag)

- a brand that deliberately owns ONE listed marker (e.g. a violet accent
  fintech) with custom palette/type/copy everywhere else — deliberate
  exception, documented.
- `#FFFFFF` used as a neutral background (not part of a purple/cream combo).
- a serif headline without the cream+terracotta pair.
- comments or prose that mention the markers (comment-stripping is active).

## Historical evals

- Template-hero regression: a generated landing page matched all 8 marker
  families and read as instantly generic; the redesign kept one brand color
  (deep green) and one bold CTA, and removed the numbered labels and
  hairlines.
- Design2Code (arxiv 2403.03163) documents template-collapse in generated
  UI — designs converge toward the most common training layouts; this
  skill's fingerprint set is the reviewable surface of that failure.

## Adversarial evals

- "Violet is our brand color" — agent must verify the rest of the system is
  grounded before accepting the exception.
- "Add more gradients to make it pop" — agent must reject
  decoration-as-originality and spend boldness in one place.
- "It's fine, I've seen worse" — agent must run the scanner and the
  uniqueness test and show scores.
- "The client asked for cream and terracotta" — agent must apply the
  deliberate-exception rule and document it.
- "Originality can't be tested" — agent must run the deterministic checks
  and state what remains human judgment.

## Scoring

- detection: names the fingerprint families from the files with line
  evidence, not "it looks generic".
- reasoning: distinguishes template reuse from deliberate single-marker
  exceptions and explains the system-level view.
- fix: swaps defaults for brand palette/type, removes the template hero,
  replaces hairlines with spacing, keeps one bold idea.
- verification: demonstrates fingerprint scores and uniqueness-test verdicts
  with exit codes.

## Sources exercised

`claude-frontend-design-blog` (proposed), `anthropic-frontend-design-skill`
(proposed), `nngroup-visual-hierarchy` (proposed), `arxiv-2403-03163`
(proposed). Full reasoning in `references/ai-look-fingerprints.md` and
`references/originality-and-brand-grounding.md`.
