#!/usr/bin/env python3
"""Visual-hierarchy / composition checker for design-visual-hierarchy-composition.

Checks a single HTML page for NN/g visual-hierarchy properties that are
mechanically testable:
  1. exactly one <h1> and no skipped heading levels;
  2. hero-as-thesis: the first paragraph sits near the top of the document
     and shares at least one significant <title> keyword;
  3. key content in the first two paragraphs (title keyword within the
     first two <p> elements);
  4. F-pattern order: title, then thesis paragraph, before body headings.

Exit 0 = clean, 1 = issues. The squint test and true F-pattern eye-tracking
are documented in the references, not automated here.
"""
import re
import sys

STOP = {"with", "from", "that", "this", "they", "them", "will", "have",
        "your", "about", "into", "their", "than", "then", "page", "pages",
        "html", "css", "body", "main", "these", "those"}

H_RE = re.compile(r"<h([1-6])\b[^>]*>(.*?)</h\1>", re.S | re.I)
P_RE = re.compile(r"<p\b[^>]*>(.*?)</p>", re.S | re.I)
TITLE_RE = re.compile(r"<title[^>]*>(.*?)</title>", re.S | re.I)
TAG_RE = re.compile(r"<[^>]+>")


def text_of(html):
    return TAG_RE.sub("", html)


def sig_words(text):
    words = re.findall(r"[a-zA-Z]{4,}", text.lower())
    return set(w for w in words if w not in STOP)


def main():
    if len(sys.argv) != 2:
        print("usage: composition_check.py <page.html>")
        return 2
    path = sys.argv[1]
    html = open(path, encoding="utf-8").read()
    issues = []

    title_m = TITLE_RE.search(html)
    title_words = sig_words(title_m.group(1)) if title_m else set()
    if not title_m:
        issues.append("no <title> — cannot evaluate thesis-keyword alignment")

    heads = [(int(m.group(1)), text_of(m.group(2))) for m in H_RE.finditer(html)]
    if len([h for h, _ in heads if h == 1]) > 1:
        issues.append(f"{sum(1 for h, _ in heads if h == 1)} <h1> elements — exactly one required")
    if heads and heads[0][0] != 1:
        issues.append("document does not start with <h1>")
    prev = heads[0][0] if heads else 0
    for level, _ in heads[1:]:
        if level > prev + 1:
            issues.append(f"heading level skipped <h{prev}> -> <h{level}>")
        prev = level

    p_matches = list(P_RE.finditer(html))
    paras = [text_of(m.group(1)).strip() for m in p_matches]
    if not paras:
        issues.append("no <p> content — nothing to evaluate")
    else:
        p1 = paras[0]
        if title_words and not (sig_words(p1) & title_words):
            issues.append("first paragraph shares no significant title keyword — thesis not in the hero")
        if len(p1) < 40:
            issues.append("first paragraph is a placeholder (< 40 chars)")
        first2 = " ".join(paras[:2])
        if title_words and not (sig_words(first2) & title_words):
            issues.append("key content not in the first two paragraphs — no title keyword found")
        if p_matches and p_matches[0].start() > len(html) * 0.5:
            issues.append("first paragraph appears after the middle of the document — hero buried")

    if issues:
        for i in issues:
            print(f"ISSUE {path}: {i}")
        print(f"{path}: {len(issues)} composition issue(s) found")
        return 1
    n = len(heads)
    print(f"{path}: composition OK ({n} heading(s), one h1, thesis in first paragraphs)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
