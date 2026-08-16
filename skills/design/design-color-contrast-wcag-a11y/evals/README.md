# Evaluation — design-color-contrast-wcag-a11y

Skill: `skills/design/design-color-contrast-wcag-a11y`. Stability target:
`evaluated`. Source files: `examples/good/palette.css`,
`examples/bad/palette.css`, `examples/good/page.html`, `examples/bad/page.html`,
`examples/contrast_check.py`, `examples/a11y_attributes_check.py`.

## Host and tooling

Verification host: Windows, Python 3.11.9. Contrast math and HTML-attribute
checks are pure-python (verified below). axe-core and Lighthouse require a
browser/URL and are NOT executed here (documented commands, marked
UNVERIFIED).

## Verification commands and actual output

```
python examples/contrast_check.py examples/good/palette.css
python examples/contrast_check.py examples/bad/palette.css
python examples/a11y_attributes_check.py examples/good/page.html
python examples/a11y_attributes_check.py examples/bad/page.html
```

Observed output:

```
$ python examples/contrast_check.py examples/good/palette.css
PASS #111827 on #FFFFFF (text): 17.74:1 (need 4.5:1)
PASS #4B5563 on #FFFFFF (large): 7.56:1 (need 3.0:1)
PASS #2563EB on #FFFFFF (large): 5.17:1 (need 3.0:1)
PASS #4F46E5 on #F5F7FF (ui): 5.88:1 (need 3.0:1)
OK   a.link: padding-based target est 31px tall
OK   .button: target 24x24 px >= 24x24
examples\good\palette.css: all contrast pairs and targets pass
$ echo $LASTEXITCODE
0
$ python examples/contrast_check.py examples/bad/palette.css
FAIL #FF0000 on #FFFFFF (text): 4.00:1 (need 4.5:1)
FAIL #999999 on #FFFFFF (text): 2.85:1 (need 4.5:1)
FAIL #CCCCCC on #FFFFFF (ui): 1.61:1 (need 3.0:1)
ISSUE examples\bad\palette.css: target .tab: interactive element under 24x24 CSS px (w=20, h=20) — WCAG 2.2 2.5.8
ISSUE examples\bad\palette.css: target input.submit: interactive element under 24x24 CSS px (w=16, h=16) — WCAG 2.2 2.5.8
examples\bad\palette.css: 5 issue(s) found
$ echo $LASTEXITCODE
1
$ python examples/a11y_attributes_check.py examples/good/page.html
examples\good\page.html: a11y attributes OK (lang, title, img alt, labelled controls)
$ echo $LASTEXITCODE
0
$ python examples/a11y_attributes_check.py examples/bad/page.html
ISSUE examples\bad\page.html: <html> missing lang attribute
ISSUE examples\bad\page.html: missing <title> element (WCAG 2.4.2)
ISSUE examples\bad\page.html: <img> #1 missing alt attribute (WCAG 1.1.1)
ISSUE examples\bad\page.html: <img> #2 missing alt attribute (WCAG 1.1.1)
ISSUE examples\bad\page.html: <input> #1 has no accessible label (aria-label/label for)
ISSUE examples\bad\page.html: <input> #2 has no accessible label (aria-label/label for)
examples\bad\page.html: 6 a11y issue(s) found
$ echo $LASTEXITCODE
1
```

## Verified facts

| Fact | Status | Evidence |
|---|---|---|
| WCAG luminance/contrast math implemented and matching WebAIM math | KNOWN (observed) | #111827/#FFF = 17.74:1; #F00/#FFF = 4.00:1; #999/#FFF = 2.85:1; #CCC/#FFF = 1.61:1 |
| 4.5:1 / 3:1 / 3:1 thresholds enforced per role | KNOWN (observed) | red/gray fail text (4.5), #CCC fails ui (3.0) |
| large-text classification (18pt) at 3:1 | KNOWN (observed) | #4B5563/#FFF = 7.56:1 passes large |
| 24×24 target minimum enforced (WCAG 2.2 2.5.8) | KNOWN (observed) | 20×20 tab and 16×16 submit flagged; 24×24 button and padding link pass |
| lang/title/alt/labels detected | KNOWN (observed) | bad HTML: 6 issues; good HTML clean |
| axe-core / Lighthouse runs | UNVERIFIED | no browser on host; exact commands documented (`npx --yes @axe-core/cli <url> --tags wcag2a,wcag2aa,wcag22aa`, `npx lighthouse <url> --only-categories=accessibility`) |

## Synthetic evals

- **easy**: `#999999` on white for 16px body — must fail 4.5:1 (computed 2.85:1).
- **easy**: same pair as large text ≥18pt — must pass 3:1.
- **medium**: a brand pair that passes as UI (3:1) but fails as normal text —
  agent must classify the use before deciding.
- **medium**: `<img>` without `alt` — flagged; `alt=""` + `aria-hidden` —
  not flagged.
- **hard**: a 22×22 button with 2px extra hit area via `::after` — agent must
  recognize spacing/expansion rules of 2.5.8, not just declared box size.

## False-positive evals (must NOT flag)

- decorative image with `alt=""` and `aria-hidden="true"` (correct per 1.1.1).
- a 44×44 primary action target (exceeds requirement).
- `#4F46E5 on #F5F7FF` at 5.88:1 as a UI element — passes 3:1, must not flag.
- a heading at line-height 1.2 — not a contrast concern (that is typography).
- a disabled control intentionally at reduced contrast that includes a text
  or icon alternative (documented exception pattern).

## Historical evals

- WebAIM Million (2020–2024): low contrast is the #1 most common
  accessibility failure across ~1M home pages. A palette audit of a
  generated landing page reproduces the class: muted grays (#999) and pure
  red used as text on white.
- From Code to Compliance (arxiv 2501.03572): LLM-generated front-end code
  passes visual fidelity but drops a11y contract items — the alt/label/lang
  gap is the documented failure mode this skill's attribute checker targets.

## Adversarial evals

- "These grays look fine on my monitor" — agent must compute the ratio and
  show the number, not argue aesthetics.
- "3:1 is fine, we checked" for 16px body text — agent must apply 4.5:1.
- "It's just a decorative icon" — agent must check 1.4.11 and 1.1.1 anyway.
- "The contrast script is a nice-to-have" — agent must run it and record
  exit codes as evidence.
- "Ship it, we'll audit later" — agent must run the static checks now and
  document the axe/Lighthouse commands for CI.

## Scoring

- detection: names the failing pair and its computed ratio + the WCAG
  criterion (1.4.3/1.4.11/2.5.8), not "it looks off".
- reasoning: distinguishes normal vs large vs UI thresholds and explains
  target-size arithmetic.
- fix: picks a color that actually passes (recomputes), adds alt/lang/title/
  labels, enlarges targets to ≥24px.
- verification: shows the computed ratios and checker exit codes; documents
  browser-based commands as future CI steps.

## Sources exercised

`wcag-22` (proposed), `webaim-contrast-api` (proposed), `webaim-million`
(proposed), `lighthouse-a11y-scoring` (proposed), `arxiv-2501-03572`
(proposed). Full reasoning in `references/contrast-math.md` and
`references/wcag-22-a11y-contract.md`.
