# Evaluation — design-token-system-discipline

Skill: `skills/design/design-token-system-discipline`. Stability target:
`evaluated`. Source files: `examples/good/tokens.json`,
`examples/bad/tokens.json`, `examples/good/component.css`,
`examples/bad/component.css`, `examples/good/build_tokens.py`,
`examples/verify_dtcg.py`, `examples/verify_no_raw_literals.py`.

## Host and tooling

Verification host: Windows, Python 3.11.9, Node v26.4.0. style-dictionary
was fetched on demand with `npx` (registry reachable; `style-dictionary@4.0.2`
does not exist — latest is 5.5.1, which was used and verified).

## Verification commands and actual output

```
python examples/verify_dtcg.py examples/good/tokens.json
python examples/verify_dtcg.py examples/bad/tokens.json
python examples/verify_no_raw_literals.py examples/good/component.css
python examples/verify_no_raw_literals.py examples/bad/component.css
python examples/good/build_tokens.py examples/good/tokens.json
npx --yes style-dictionary@5.5.1 build --config examples/sd.config.json
```

Observed output:

```
$ python examples/verify_dtcg.py examples/good/tokens.json
examples\good\tokens.json: DTCG structure OK (21 tokens, all aliases resolve)
$ echo $LASTEXITCODE
0
$ python examples/verify_dtcg.py examples/bad/tokens.json
ISSUE examples\bad\tokens.json: semantic.color.text.primary: semantic token must alias a primitive, got raw value '#1F2937'
ISSUE examples\bad\tokens.json: semantic.color.text.muted: alias target does not exist: {color.brand.999}
ISSUE examples\bad\tokens.json: button.bg: missing $type
examples\bad\tokens.json: 3 issue(s) found
$ echo $LASTEXITCODE
1
$ python examples/verify_no_raw_literals.py examples/good/component.css
examples\good\component.css: no raw literals — token-first CSS OK
$ echo $LASTEXITCODE
0
$ python examples/verify_no_raw_literals.py examples/bad/component.css
RAW examples\bad\component.css:4: hex color #4F46E5
RAW examples\bad\component.css:5: hex color #111827
RAW examples\bad\component.css:6: length 8px
RAW examples\bad\component.css:6: length 16px
RAW examples\bad\component.css:7: length 16px
RAW examples\bad\component.css:8: length 6px
examples\bad\component.css: 6 raw literal(s) — use design tokens instead
$ echo $LASTEXITCODE
1
$ npx --yes style-dictionary@5.5.1 build --config examples/sd.config.json
css
✔︎ examples/good/build/tokens.css
$ echo $LASTEXITCODE
0
```

The generated `tokens.css` included 21 variables, notably
`--typography-body: 400 16px/24px 'Spline Sans', sans-serif;` (composite
token) and `--semantic-color-action: #4f46e5;` (alias chain resolved).
Generated build output was removed after recording; commands are
re-runnable with `npx`.

## Verified facts

| Fact | Status | Evidence |
|---|---|---|
| good DTCG token file is structurally valid | KNOWN (observed) | `verify_dtcg.py` exit 0, "21 tokens, all aliases resolve" |
| bad token file is rejected | KNOWN (observed) | exit 1, 3 issues: raw semantic literal, dangling alias, missing $type |
| raw literals in component CSS are detected | KNOWN (observed) | bad CSS: 6 raw literals (exit 1); good CSS clean (exit 0) |
| semantic layer must alias primitives | KNOWN (observed) | `semantic.color.text.primary` raw value rejected |
| style-dictionary 5.5.1 builds CSS from DTCG source | KNOWN (observed) | npx build exit 0, `tokens.css` emitted on Node v26.4.0 |
| composite typography tokens emit a shorthand | KNOWN (observed) | `--typography-body: 400 16px/24px 'Spline Sans', sans-serif` |
| `style-dictionary@4.0.2` version pin | INFERRED | npm reports ETARGET; latest tag is 5.5.1 (documented for reproducibility) |

## Synthetic evals

- **easy**: tokens missing `$type` or `$value` — must be reported as invalid DTCG.
- **easy**: semantic token holding a raw hex — must be rejected (semantic layer aliases only).
- **medium**: a `{color.brand.999}` alias where `brand.999` does not exist —
  must be caught by alias resolution, not by the browser.
- **medium**: composite typography token with one unresolved alias inside
  `$value` — must be caught by the deep walk.
- **hard**: alias chains (semantic → primitive → value) with a cycle — the
  build must fail loudly, not recurse forever.

## False-positive evals (must NOT flag)

- a `@font-face` `src` url() and `local()` descriptor (not a token value).
- media-query breakpoints (`@media (min-width: 768px)`) — layout logic, not
  a spacing token.
- a demo/throwaway page that is explicitly outside the token system and
  defines its own one-off values with a comment saying so.
- `var(--x, fallback)` usage where the fallback is explicitly documented as
  a degradation path, not a duplicate source of truth.

## Historical evals

- Rebranding incident class: brand color shipped as raw hex in 40 components
  while tokens.json still had the old brand value — after the "rebrand", the
  two sets diverged. Agent must trace every literal to a token.
- DTCG adoption failure: teams keep `tokens.json` but hand-maintain
  `variables.css` — the fix is a build step, not a sync ritual.

## Adversarial evals

- "It's the brand color, just inline it once" — agent must route through the
  semantic layer and refuse the shortcut.
- "The build failed, delete the `$type` fields" — agent must fix the format;
  stripping `$type` is destroying the contract.
- "Add a hard-coded fallback color next to the var()" — agent must reject
  duplicated values that will drift.
- "This token file is fine, the browser renders it" — agent must run the
  validator and the raw-literal scanner and show the evidence.

## Scoring

- detection: names the defect class (missing $type / dangling alias / raw
  semantic literal / component literal) from the file, not from vibes.
- reasoning: explains why semantic→primitive aliasing keeps theming stable.
- fix: adds the missing token, rewrites raw semantic values as aliases, and
  re-runs the build.
- verification: demonstrates exit codes and the generated `tokens.css`
  output rather than claiming "it parses".

## Sources exercised

`dtcg-design-tokens` (proposed), `anthropic-frontend-design-skill`
(proposed), `claude-frontend-design-blog` (proposed), `arxiv-2403-03163`
(proposed). Full reasoning in `references/token-hierarchy.md` and
`references/dtcg-format.md`.
