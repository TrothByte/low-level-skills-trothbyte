#!/usr/bin/env python3
"""AI-look fingerprint scanner for design-anti-ai-look-originality-review.

Scans a page (HTML + CSS) for the documented AI-slop markers and counts how
many DISTINCT fingerprint families match:

  1. default font stacks (Inter, Roboto, system-ui, -apple-system)
  2. purple/violet gradients on white backgrounds
  3. cream + serif + terracotta combo (#F4F1EA palette)
  4. near-black + acid-green combo
  5. broadsheet hairline dividers (1px solid #E5E7EB)
  6. Space Grotesk headings drift
  7. numbered 01/02/03 labels
  8. template hero: h1 + subhead + gradient CTA + >= 3 card-like elements

Score = distinct families matched. Exit 0 = < 3 (original enough),
exit 1 = >= 3 (AI-look probable; "spend your boldness in one place").
"""
import re
import sys

DEFAULT_FONTS = ("inter", "roboto", "system-ui", "-apple-system",
                 "blinkmacsystemfont", "segoe ui")
PURPLE = re.compile(
    r"#[7C][C3][Aa][Ee][Dd]|#[8B][bB]5[Cc][Ff]6|#[Aa]855[Ff]7|#6366[Ff]1|"
    r"rgb\(\s*124\s*,\s*58\s*,\s*237\s*\)|rgb\(\s*139\s*,\s*92\s*,\s*246\s*\)|"
    r"violet|purple", re.I)
GRADIENT = re.compile(r"gradient\s*\(", re.I)
WHITE_BG = re.compile(r"background(-color)?\s*:\s*(#(?:FFF|FFFFFF|FAFAFA|F9FAFB)\b|white|#fff)", re.I)
CREAM = re.compile(r"#F4F1EA|#f4f1ea")
TERRACOTTA = re.compile(r"#E07A5F|#E76F51|#C65D47|#D9755B", re.I)
SERIF_FACE = re.compile(r"Georgia|Playfair|Times New Roman|\bserif\b", re.I)
NEAR_BLACK = re.compile(r"#0F172A|#0D0D0D|#111827|#1A1A1A", re.I)
ACID_GREEN = re.compile(r"#A3E635|#B4FF39|#22C55E|#84CC16", re.I)
HAIRLINE = re.compile(r"border-(?:top|bottom)\s*:\s*1px\s+solid\s+#[Ee]5[Ee]7[Ee][Bb]", re.I)
SPACE_GROTESK = re.compile(r"Space\s+Grotesk", re.I)
NUM_LABEL = re.compile(r"\b(?:step|num|item)-0[123]\b|>0[123]<", re.I)
CARD_CLASS = re.compile(r"\b(card|feature|service|benefit|pillar|step|metric)\b", re.I)


def hit_lines(text, regex):
    return [i for i, line in enumerate(text.splitlines(), 1) if regex.search(line)]


def strip_comments(html, css):
    html = re.sub(r"<!--.*?-->", "", html, flags=re.S)
    css = re.sub(r"/\*.*?\*/", "", css, flags=re.S)
    return html, css


def main():
    if len(sys.argv) != 3:
        print("usage: ai_look_fingerprint.py <page.html> <style.css>")
        return 2
    html_path, css_path = sys.argv[1], sys.argv[2]
    html = open(html_path, encoding="utf-8").read()
    css = open(css_path, encoding="utf-8").read()
    html, css = strip_comments(html, css)
    all_text = html + "\n" + css
    found = []

    fams = hit_lines(css, re.compile(
        r"font-family\s*:\s*([^;]+)", re.I))
    default_hits = []
    for ln in fams:
        m = re.search(r"font-family\s*:\s*([^;]+)", css.splitlines()[ln - 1], re.I)
        if m:
            first = m.group(1).split(",")[0].strip().strip("\"'").lower()
            if first in DEFAULT_FONTS:
                default_hits.append(ln)
    if default_hits:
        found.append(("default font stack (Inter/Roboto/system-ui)", default_hits))

    purple_grad = set()
    for ln in hit_lines(all_text, PURPLE):
        line = all_text.splitlines()[ln - 1]
        if GRADIENT.search(line):
            purple_grad.add(ln)
    if purple_grad:
        found.append(("purple/violet gradient", sorted(purple_grad)))
    elif hit_lines(css, PURPLE) and hit_lines(css, WHITE_BG):
        found.append(("purple on white (flat)", hit_lines(css, PURPLE)))

    if hit_lines(all_text, CREAM) and hit_lines(css, SERIF_FACE) and hit_lines(all_text, TERRACOTTA):
        found.append(("cream + serif + terracotta (#F4F1EA combo)", hit_lines(all_text, CREAM)))

    nb = set(hit_lines(all_text, NEAR_BLACK))
    ag = set(hit_lines(all_text, ACID_GREEN))
    if nb and ag:
        found.append(("near-black + acid-green combo", sorted(nb | ag)))

    hl = hit_lines(all_text, HAIRLINE)
    if hl:
        found.append(("broadsheet hairline dividers (1px #E5E7EB)", hl))

    sg = hit_lines(all_text, SPACE_GROTESK)
    if sg:
        found.append(("Space Grotesk drift", sg))

    nl = hit_lines(all_text, NUM_LABEL)
    if nl:
        found.append(("numbered 01/02/03 labels", nl))

    # template hero: h1 + subhead + gradient CTA + >= 3 card-like elements
    h1 = re.search(r"<h1\b", html, re.I)
    sub = re.search(r"class=\"[^\"]*subhead|</h1>\s*<p", html, re.I)
    grad_cta = False
    for ln in hit_lines(css, re.compile(r"\.(cta|btn|button)[^{}]*\{[^{}]*gradient", re.I | re.S)):
        grad_cta = True
    # fallback: any rule whose selector mentions cta and body has gradient
    if not grad_cta:
        for m in re.finditer(r"([^{}]+)\{([^{}]*)\}", css, re.S):
            if re.search(r"cta|btn|button", m.group(1), re.I) and "gradient" in m.group(2):
                grad_cta = True
                break
    cards = len(re.findall(r'class="[^"]*\b(?:card|feature|step|service|benefit|pillar)\b[^"]*"', html, re.I))
    if h1 and sub and grad_cta and cards >= 3:
        found.append(("template hero (h1 + subhead + gradient CTA + 3 cards)", [1]))

    for name, lines in found:
        print(f"FINGERPRINT {name}: lines {lines}")

    if found:
        print(f"score: {len(found)} distinct AI-look fingerprints")
    if len(found) >= 3:
        print(f"{html_path}: AI-look probable — reduce to one bold idea")
        return 1
    print(f"{html_path}: original enough ({len(found)} fingerprints)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
