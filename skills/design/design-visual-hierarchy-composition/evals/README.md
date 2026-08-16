# Evaluation — design-visual-hierarchy-composition

Skill: `skills/design/design-visual-hierarchy-composition`. Stability target:
`evaluated`. Source files: `examples/good/index.html`,
`examples/bad/index.html`, `examples/composition_check.py`.

## Host and tooling

Verification host: Windows, Python 3.11.9. The structural/thesis checks are
pure-python (verified below). The squint test and F-pattern eye-tracking
are human/browser methods — documented, not automated here.

## Verification commands and actual output

```
python examples/composition_check.py examples/good/index.html
python examples/composition_check.py examples/bad/index.html
```

Observed output:

```
$ python examples/composition_check.py examples/good/index.html
examples\good\index.html: composition OK (4 heading(s), one h1, thesis in first paragraphs)
$ echo $LASTEXITCODE
0
$ python examples/composition_check.py examples/bad/index.html
ISSUE examples\bad\index.html: 2 <h1> elements — exactly one required
ISSUE examples\bad\index.html: heading level skipped <h1> -> <h3>
ISSUE examples\bad\index.html: heading level skipped <h3> -> <h5>
ISSUE examples\bad\index.html: first paragraph shares no significant title keyword — thesis not in the hero
ISSUE examples\bad\index.html: key content not in the first two paragraphs — no title keyword found
examples\bad\index.html: 5 composition issue(s) found
$ echo $LASTEXITCODE
1
```

## Verified facts

| Fact | Status | Evidence |
|---|---|---|
| exactly-one-h1 enforced | KNOWN (observed) | 2 `<h1>`s flagged |
| heading-level no-skip enforced | KNOWN (observed) | `h1->h3`, `h3->h5` flagged |
| hero-as-thesis (title keyword in p1) | KNOWN (observed) | "thesis not in the hero" for bad fixture |
| key content in first two paragraphs | KNOWN (observed) | "key content not in the first two paragraphs" for bad fixture |
| hero position in document order | KNOWN (observed) | first `<p>` must precede 50% of document |
| F-pattern eye-tracking behavior | INFERRED | NN/g research cited, not reproduced on host |
| squint test | UNVERIFIED (manual) | human visual method; documented |

## Synthetic evals

- **easy**: two `<h1>`s — flagged.
- **easy**: `h1` → `h3` skip — flagged.
- **medium**: lorem ipsum first two paragraphs with the real thesis in
  paragraph five — flagged via title-keyword absence.
- **medium**: a page whose h1 matches the title but whose first paragraph is
  a 15-char tagline — flagged as placeholder (< 40 chars).
- **hard**: a single-page app using `role="heading"` with aria-level instead
  of `h1`–`h6` — agent must recognize the authored-HTML scope and check
  roles separately.

## False-positive evals (must NOT flag)

- a page with no `<title>` keyword overlap in the hero when the title is
  generic ("Home") — the keyword heuristic is a proxy, and absence of title
  match is a warning class, not the whole verdict.
- a deliberately telegraphic hero (a 30-char product line) when the CTA and
  h2 clearly carry the message.
- a document with zero `<p>` elements (a form-only page) — the paragraph
  checks skip when no paragraphs exist.

## Historical evals

- NN/g F-pattern findings: horizontal scanning of the first line, weaker
  second line, vertical scan down the left edge. Redesigns that moved key
  content into the first line of body copy measurably improved task success
  (documented research, not reproduced here).
- Buried-thesis regression: a generated company page opened with two lorem
  paragraphs and welcomed visitors with "Welcome" + "Our Company" as two
  h1s; the fix moved the value statement into paragraph one.

## Adversarial evals

- "Every section is equally important" — agent must force a ranking and
  refuse flat hierarchy.
- "Add a second h1 for the brand" — agent must route the brand into the
  single h1 or an `<h2>`/logo.
- "The lorem ipsum will be replaced later" — agent must not ship placeholder
  as the thesis.
- "Users read everything anyway" — agent must cite F-pattern evidence and
  enforce the first-two-paragraph rule.

## Scoring

- detection: names the structural defect (multiple h1 / skip / buried
  thesis / placeholder hero) from the HTML.
- reasoning: explains outline semantics (WCAG 1.3.1) and F-pattern placement.
- fix: restores single h1 + ordered levels, moves the thesis into paragraph
  one, removes filler.
- verification: demonstrates checker exit codes and issue lists; documents
  the squint test as the manual confirmation.

## Sources exercised

`nngroup-visual-hierarchy` (proposed), `nngroup-f-pattern` (proposed),
`wcag-22` (proposed), `anthropic-frontend-design-skill` (proposed),
`claude-frontend-design-blog` (proposed). Full reasoning in
`references/visual-hierarchy-nng.md` and `references/composition-and-f-pattern.md`.
