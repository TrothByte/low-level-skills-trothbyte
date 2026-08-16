# Evaluation — design-typography-hierarchy

Skill: `skills/design/design-typography-hierarchy`. Stability target:
`evaluated`. Source files: `examples/good/type.css`, `examples/bad/type.css`,
`examples/good/index.html`, `examples/bad/index.html`,
`examples/type_scale_check.py`, `examples/heading_level_check.py`,
`examples/font_stack_check.py`.

## Host and tooling

Verification host: Windows, Python 3.11.9. All checks are pure-python
(no browser). Browser-based verification (rendered 320px screenshot checks)
is documented but NOT executed here.

## Verification commands and actual output

```
python examples/type_scale_check.py examples/good/type.css
python examples/type_scale_check.py examples/bad/type.css
python examples/heading_level_check.py examples/good/index.html
python examples/heading_level_check.py examples/bad/index.html
python examples/font_stack_check.py examples/good/type.css
python examples/font_stack_check.py examples/bad/type.css
```

Observed output:

```
$ python examples/type_scale_check.py examples/good/type.css
examples\good\type.css: type scale deliberate, weight contrast present, text spacing OK
$ echo $LASTEXITCODE
0
$ python examples/type_scale_check.py examples/bad/type.css
ISSUE examples\bad\type.css: 7 distinct font sizes (['11px', '12px', '14px', '15px', '19px', '23px', '34px']) — not a deliberate scale
ISSUE examples\bad\type.css: size gap 11px -> 12px (ratio 1.091) matches no modular step
ISSUE examples\bad\type.css: size gap 12px -> 14px (ratio 1.167) matches no modular step
ISSUE examples\bad\type.css: size gap 14px -> 15px (ratio 1.071) matches no modular step
ISSUE examples\bad\type.css: size gap 23px -> 34px (ratio 1.478) matches no modular step
ISSUE examples\bad\type.css: all heading weights are light (<= 300) with no 700+ anywhere — no weight contrast
ISSUE examples\bad\type.css: body: line-height 1.2 < 1.5 (WCAG 1.4.8/1.4.12)
ISSUE examples\bad\type.css: .button: line-height 1.1 < 1.5 (WCAG 1.4.8/1.4.12)
examples\bad\type.css: 8 issue(s) found
$ echo $LASTEXITCODE
1
$ python examples/heading_level_check.py examples/good/index.html
examples\good\index.html: heading structure OK (4 heading(s), exactly one h1, no skips)
$ echo $LASTEXITCODE
0
$ python examples/heading_level_check.py examples/bad/index.html
ISSUE examples\bad\index.html: 2 <h1> elements — exactly one is required
ISSUE examples\bad\index.html: heading level skipped: <h1> -> <h3> ('Features')
ISSUE examples\bad\index.html: heading level skipped: <h3> -> <h5> ('Pricing')
examples\bad\index.html: 3 heading issue(s) found
$ echo $LASTEXITCODE
1
$ python examples/font_stack_check.py examples/good/type.css
examples\good\type.css: no default font stacks — brand/custom family leads
$ echo $LASTEXITCODE
0
$ python examples/font_stack_check.py examples/bad/type.css
ISSUE examples\bad\type.css: default font stack starts with 'inter': inter, -apple-system, blinkmacsystemfont, segoe ui, roboto
examples\bad\type.css: 1 default-stack issue(s) found
$ echo $LASTEXITCODE
1
```

## Verified facts

| Fact | Status | Evidence |
|---|---|---|
| good 1.25-ratio scale (12.8/16/20/25/31.25 px) passes | KNOWN (observed) | `type_scale_check.py` exit 0 |
| bad 7-size arbitrary ramp is rejected | KNOWN (observed) | exit 1, 8 issues (scale, gaps, weights, line-height) |
| light-only weights (100-300) flagged | KNOWN (observed) | "no weight contrast" issue emitted |
| body line-height < 1.5 flagged | KNOWN (observed) | 1.2 and 1.1 flagged per WCAG 1.4.8/1.4.12 |
| duplicate h1 / skipped levels detected | KNOWN (observed) | `heading_level_check.py` exit 1, 3 issues |
| Inter/system default stacks flagged | KNOWN (observed) | `font_stack_check.py` exit 1 on bad, 0 on good |
| "≤ 3 sizes per screen" exact number | INFERRED | NN/g style guidance; checker enforces 7 as firm smell threshold |
| 320px reflow of type | UNVERIFIED | browser-rendered; documented (see `design-layout-spacing-grid`) |

## Synthetic evals

- **easy**: font sizes 34/23/19/15 px — must be flagged as off-scale.
- **easy**: `line-height: 1.2` on `p` — must be flagged (WCAG 1.4.8/1.4.12).
- **medium**: a scale built with ratio 1.333 instead of 1.25 — must pass if
  the gaps match 1.333 steps (ratio-agnostic checker).
- **medium**: two `<h1>` elements — must be flagged; exactly one required.
- **hard**: `h1` → `h2` → `h4` with an intervening `h3` inside a nested
  component — must still flag the `h2`→`h4` skip in document order.

## False-positive evals (must NOT flag)

- heading `line-height: 1.2` (headings are exempt from the 1.5 requirement).
- a brand family leading a stack (`"Spline Sans", sans-serif`).
- a `clamp()` display size that does not sit on the numeric scale (fluid role).
- a caption at 0.8rem used consistently.
- a one-off inline style in a demo page that is explicitly out of the token
  system.

## Historical evals

- AI-slop regression: a generated landing page whose headline was set to
  34px light Inter "to look premium" with body line-height 1.2; the fix was
  rewriting the ramp from a 1.25 scale (12.8/16/20/25/31.25), 400/700
  weights, and line-height 1.6.
- Heading-skip incident: section content assembled from separate components
  produced `h2` then `h4`; the fix routed section titles through a shared
  heading component.

## Adversarial evals

- "Just one more size for this statistic" — agent must fit it to the scale
  or refuse.
- "The mock uses 17px, keep it" — agent must snap to the scale and flag the
  mock rather than the scale.
- "Light 100-weight tagline for premium feel" — agent must pair light with
  heavy or reject.
- "The Inter stack is fine, everyone uses it" — agent must run
  `font_stack_check.py` and show the fingerprint hit.

## Scoring

- detection: names the defect class (off-scale size / light-only weights /
  line-height / skipped heading / default stack) from the file.
- reasoning: explains scale step math and why heading exemptions apply.
- fix: rewrites the ramp from the scale, adds weight contrast, fixes
  line-height, restores h1/h2/h3 order.
- verification: demonstrates checker exit codes and issue lists, not "it
  looks fine".

## Sources exercised

`wcag-22` (proposed), `nngroup-visual-hierarchy` (proposed),
`modular-scale` (proposed), `utopia-fluid-type` (proposed),
`anthropic-frontend-design-skill` (proposed), `claude-frontend-design-blog`
(proposed). Full reasoning in `references/type-scale-and-roles.md` and
`references/text-spacing-wcag.md`.
