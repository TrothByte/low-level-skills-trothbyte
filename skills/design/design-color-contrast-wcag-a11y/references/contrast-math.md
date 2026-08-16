# WCAG Contrast Math

Rule format: RULE / WHY AI GETS IT WRONG / CORRECT REASONING / EXAMPLE /
COUNTEREXAMPLE / VERIFICATION / SOURCE. Facts marked **VERIFIED** were
observed on this host (Python 3.11.9) with `examples/contrast_check.py`.

## 1. Contrast is computed from relative luminance, not perception

- **RULE**: relative luminance is L = 0.2126R + 0.7152G + 0.0722B where each
  sRGB channel c is linearized: c/12.92 if c ≤ 0.03928, else
  ((c + 0.055)/1.055)^2.4. Contrast ratio = (L_hi + 0.05)/(L_lo + 0.05).
  There is no valid "eyeball" shortcut.
- **WHY AI GETS IT WRONG**: agents estimate "this looks readable" and commit
  to a pass verdict — the classic self-eval divergence. Color pairs that
  look distinct (red on white, gray on white) routinely fail.
- **CORRECT REASONING**: the ratio is a deterministic function of two hex
  values. Compute it; if a pair is under the threshold, change the color,
  not the verdict.
- **EXAMPLE**: `#111827` on `#FFFFFF` → 17.74:1.
- **COUNTEREXAMPLE**: `#FF0000` on `#FFFFFF` → 4.00:1 (fails normal text),
  `#999999` on `#FFFFFF` → 2.85:1 (fails), despite "looking fine".
- **VERIFICATION**: VERIFIED — `contrast_check.py` computed exactly 17.74,
  4.00, 2.85, 1.61 for the fixture pairs on this host (Python 3.11.9).
- **SOURCE**: https://www.w3.org/TR/WCAG22/ — 1.4.3 (proposed `wcag-22`); https://webaim.org/resources/contrastchecker/ (proposed `webaim-contrast-api`)

## 2. Threshold depends on use: 4.5:1 / 3:1 / 3:1

- **RULE**: normal text needs 4.5:1 (1.4.3). Large text (≥18pt/24px, or
  ≥14pt/18.66px bold) needs 3:1. Non-text UI components and graphical
  objects need 3:1 (1.4.11). A single 3:1 number applied everywhere is wrong.
- **WHY AI GETS IT WRONG**: the agent remembers "3:1" as the contrast rule
  and applies it to body text.
- **CORRECT REASONING**: classify the pair first (normal text / large text /
  UI), then apply that class's threshold. The same pair can pass as UI but
  fail as normal text.
- **EXAMPLE**: `#2563EB` on `#FFFFFF` = 5.17:1 — passes both large-text 3:1
  and even normal-text 4.5:1.
- **COUNTEREXAMPLE**: `#CCCCCC` on `#FFFFFF` = 1.61:1 used for a tab's
  unselected state — fails the 3:1 UI requirement.
- **VERIFICATION**: VERIFIED — the checker enforces `text 4.5 / large 3.0 /
  ui 3.0` and reports the role with every ratio.
- **SOURCE**: https://www.w3.org/TR/WCAG22/ — 1.4.3, 1.4.11 (proposed `wcag-22`)

## 3. Large text is size- and weight-defined

- **RULE**: "large" = ≥ 18pt (~24px) regular, or ≥ 14pt (~18.66px) bold
  (WCAG 1.4.3 definition, px per WCAG CSS reference pixel ~1.333).
- **WHY AI GETS IT WRONG**: a 16px heading is treated as "large" because it
  is a heading; size, not semantics, sets the threshold.
- **CORRECT REASONING**: the 3:1 concession exists because larger glyphs are
  easier to read; a 16px paragraph and a 16px heading are both normal text.
- **EXAMPLE**: `.muted-large { font-size: 18pt; }` at 7.56:1 passes 3:1 as
  large text — and still passes 4.5:1.
- **COUNTEREXAMPLE**: `p { font-size: 15px; color: #999; }` on white — 2.85:1,
  fails normal text despite the muted "hint" role.
- **VERIFICATION**: KNOWN from WCAG 1.4.3 definitions; the fixture applies
  the 18pt case and the checker passes it at 3:1 while failing the 15px case.
- **SOURCE**: https://www.w3.org/TR/WCAG22/ — 1.4.3 (proposed `wcag-22`); https://webaim.org/resources/contrastchecker/ (proposed `webaim-contrast-api`)
