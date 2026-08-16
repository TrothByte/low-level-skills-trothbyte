# DTCG Design Token Format

Rule format: RULE / WHY AI GETS IT WRONG / CORRECT REASONING / EXAMPLE /
COUNTEREXAMPLE / VERIFICATION / SOURCE. Facts marked **VERIFIED** were
observed on this host (Python 3.11.9) with the fixtures under `examples/`.

## 1. Every token carries `$type` and `$value`

- **RULE**: in DTCG format, each non-group node is `{ "$type": T, "$value": V }`
  where T is a registered type (color, dimension, fontFamily, typography, ...)
  and V is the value or an alias. A token missing either field is invalid.
- **WHY AI GETS IT WRONG**: agents emit flat objects of name→value pairs and
  only add `$type` when the linter complains; the file is then neither valid
  DTCG nor consumable by tooling such as style-dictionary.
- **CORRECT REASONING**: `$type` lets tooling validate and transform values
  (hex→rgba, px→rem). Missing types mean the build must guess, which is where
  silent wrongness enters.
- **EXAMPLE**: `"brand.500": { "$type": "color", "$value": "#4F46E5" }`.
- **COUNTEREXAMPLE**: `"button.bg": { "$value": "#4F46E5" }` — no `$type`.
- **VERIFICATION**: VERIFIED — `verify_dtcg.py` reports `button.bg: missing
  $type` for `examples/bad/tokens.json` and validates the good file as
  "21 tokens, all aliases resolve" (exit 0).
- **SOURCE**: https://tr.designtokens.org/format/ (proposed `dtcg-design-tokens`)

## 2. Aliases reference the full path `{group.subgroup.token}`

- **RULE**: an alias value is the string `{path.to.token}` and must resolve
  to an existing token path in the same file. Dangling aliases are defects;
  aliases may chain (semantic → primitive → value).
- **WHY AI GETS IT WRONG**: the agent invents a pretty name for the target
  (`{color.brand.999}`) while writing the file, and the error only surfaces
  at build time — or never, if the build is skipped.
- **CORRECT REASONING**: alias resolution is a deterministic graph operation.
  A validator can check it exhaustively; if it does not resolve, the token
  file is broken and must be fixed before any CSS is generated.
- **EXAMPLE**: `"action": { "$type": "color", "$value": "{color.brand.500}" }`.
- **COUNTEREXAMPLE**: `"muted": { "$type": "color", "$value": "{color.brand.999}" }`
  where `brand.999` does not exist.
- **VERIFICATION**: VERIFIED — `verify_dtcg.py` rejects the dangling alias
  `{color.brand.999}` in `examples/bad/tokens.json` (3 issues total, exit 1).
- **SOURCE**: https://tr.designtokens.org/format/ (proposed `dtcg-design-tokens`)

## 3. Composite tokens hold an object `$value`

- **RULE**: composite types (typography, border, shadow, gradient) store a
  structured object in `$value`; each member may itself be an alias or a
  nested primitive, and all aliases must resolve.
- **WHY AI GETS IT WRONG**: the agent flattens a typography token into
  separate `fontSize`/`fontWeight`/`lineHeight` tokens and drops the grouping,
  or writes a typography `$value` that is a plain string instead of an object.
- **CORRECT REASONING**: the composite token is the design-system contract
  for the whole rule (e.g. all of typography.body in one place). A build can
  then emit it as one shorthand (`400 16px/24px 'Spline Sans', sans-serif`)
  that components consume as `var(--typography-body)`.
- **EXAMPLE**: `"typography.body": { "$type": "typography", "$value": { "fontFamily": "{font.family.sans}", "fontSize": "16px", "fontWeight": 400, "lineHeight": "24px" } }`.
- **COUNTEREXAMPLE**: `"typography.body": { "$type": "typography", "$value": "16px sans-serif" }`.
- **VERIFICATION**: VERIFIED — `verify_dtcg.py` walks composite `$value`
  objects and resolves their aliases; the good file passes, and
  style-dictionary 5.5.1 emitted `--typography-body: 400 16px/24px 'Spline
  Sans', sans-serif` from it.
- **SOURCE**: https://tr.designtokens.org/format/ (proposed `dtcg-design-tokens`); https://claude.com/blog/improving-frontend-design-through-skills (proposed `claude-frontend-design-blog`)
