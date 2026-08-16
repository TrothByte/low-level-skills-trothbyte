#!/usr/bin/env python3
"""gen_social_preview.py — regenerate social-preview.png (1280x640) with current stats.

Palette: dark bg #070b12, sky #38bdf8, teal #5eead4, text #e6edf3 (verified WCAG AA).
Numbers come from registry/skills.yaml + sources.yaml + claims.yaml.
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def load_yaml(name):
    import yaml
    with open(os.path.join(ROOT, "registry", name), encoding="utf-8") as f:
        return yaml.safe_load(f)


def font_path(size):
    # Try common Windows/mono font locations; fall back to default if missing.
    candidates = [
        "C:\\Windows\\Fonts\\arialbd.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\segoeuib.ttf",
    ]
    for c in candidates:
        if os.path.isfile(c):
            try:
                return ImageFont.truetype(c, size)
            except Exception:
                continue
    return ImageFont.load_default()


def main():
    sk = load_yaml("skills.yaml")
    so = load_yaml("sources.yaml")
    cl = load_yaml("claims.yaml")
    total = sk["summary"]["total_registered"]
    sb = sk["summary"]["source_backed"]
    n_src = len(so["sources"])
    n_cl = len(cl["claims"])
    n_dom = len(set(p.split("/")[1] for s in sk["skills"] for p in [s["path"]]))

    W, H = 1280, 640
    BG = (7, 11, 18)
    TEXT = (230, 237, 243)
    TEXT2 = (159, 176, 195)
    SKY = (56, 189, 248)
    TEAL = (94, 234, 212)
    GRAY = (107, 124, 143)

    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)

    # subtle gradient bands (sky -> teal) in a corner
    for i in range(4):
        alpha = 10 + i * 5
        band = (56, 189, 248, alpha)
        d.rectangle([W - 320 + i * 8, 0, W, H], fill=(BG[0], BG[1], BG[2]))
    # soft accent diagonal
    d.line([(W, 0), (W - 260, 0)], fill=(56, 189, 248), width=8)
    d.line([(W, 0), (W, 260)], fill=(94, 234, 212), width=8)

    f_title = font_path(64)
    f_body = font_path(28)
    f_small = font_path(22)

    d.text((64, 72), "Low-level skills TrothByte", font=f_title, fill=TEXT)
    d.text((64, 168),
           f"{total} verified skills for AI coding agents",
           font=f_body, fill=TEAL)

    stats = [
        (f"{total} skills", TEXT),
        (f"{n_dom} domains", TEXT),
        (f"{sb} source-backed", SKY),
        (f"{n_src} primary sources", TEXT2),
        (f"{n_cl} traced claims", TEXT2),
    ]
    y = 290
    for label, color in stats:
        d.text((64, y), label, font=f_body, fill=color)
        y += 52

    d.text((64, H - 80),
           "Every claim traced: claim -> source -> section -> skill",
           font=f_small, fill=GRAY)
    d.text((64, H - 48),
           "https://github.com/TrothByte/low-level-skills-trothbyte",
           font=f_small, fill=GRAY)

    out = os.path.join(ROOT, "docs", "social-preview.png")
    img.save(out, "PNG")
    img.save(os.path.join(ROOT, ".github", "social-preview.png"), "PNG")
    print(f"wrote {out} ({total} skills, {n_dom} domains, {sb} source-backed, {n_src} sources, {n_cl} claims)")


if __name__ == "__main__":
    main()
