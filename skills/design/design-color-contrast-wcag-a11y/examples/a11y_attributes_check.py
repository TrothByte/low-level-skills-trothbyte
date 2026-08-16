#!/usr/bin/env python3
"""HTML accessibility-attribute checker for design-color-contrast-wcag-a11y.

Checks: <html lang> present, <title> present, every <img> has an alt
attribute, and every <input>/<select>/<textarea> has an accessible label
(aria-label, aria-labelledby, a <label for> pointing at its id, or a
wrapping <label>). Exit 0 = clean, 1 = issues.
"""
import re
import sys

LANG_RE = re.compile(r"<html\b[^>]*\blang\s*=", re.I)
TITLE_RE = re.compile(r"<title\b[^>]*>.*?</title>", re.I | re.S)
IMG_RE = re.compile(r"<img\b[^>]*>", re.I)
INPUT_RE = re.compile(r"<(input|select|textarea)\b([^>]*)>", re.I)
ID_ATTR = re.compile(r"\bid\s*=\s*[\"']([^\"']+)[\"']", re.I)
LABEL_FOR_RE = re.compile(r"<label\b[^>]*\bfor\s*=\s*[\"']([^\"']+)[\"']", re.I)
WRAP_LABEL_RE = re.compile(r"<label\b[^>]*>(.*?)</label>", re.I | re.S)


def main():
    if len(sys.argv) != 2:
        print("usage: a11y_attributes_check.py <page.html>")
        return 2
    path = sys.argv[1]
    html = open(path, encoding="utf-8").read()
    issues = []

    if not LANG_RE.search(html):
        issues.append("<html> missing lang attribute")
    if not TITLE_RE.search(html):
        issues.append("missing <title> element (WCAG 2.4.2)")
    for i, m in enumerate(IMG_RE.finditer(html), 1):
        if not re.search(r"\balt\s*=", m.group(0), re.I):
            issues.append(f"<img> #{i} missing alt attribute (WCAG 1.1.1)")

    labelled = set()
    for m in LABEL_FOR_RE.finditer(html):
        labelled.add(m.group(1))
    for m in WRAP_LABEL_RE.finditer(html):
        inner_id = ID_ATTR.search(m.group(1))
        if inner_id:
            labelled.add(inner_id.group(1))
    for i, (tag, attrs) in enumerate(INPUT_RE.findall(html), 1):
        if re.search(r"\b(aria-label|aria-labelledby)\s*=", attrs, re.I):
            continue
        mid = ID_ATTR.search(attrs)
        if mid and mid.group(1) in labelled:
            continue
        issues.append(f"<{tag}> #{i} has no accessible label (aria-label/label for)")

    if issues:
        for i in issues:
            print(f"ISSUE {path}: {i}")
        print(f"{path}: {len(issues)} a11y issue(s) found")
        return 1
    print(f"{path}: a11y attributes OK (lang, title, img alt, labelled controls)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
