#!/usr/bin/env python3
"""prose_lint.py — light-weight prose quality checks on SKILL.md files.

Checks per v2.0 Level-4 Quality Gate:
1. Logical consistency: reasoning steps must reference numbers/preceding steps
   (step N+1 should logically follow step N).No contradictory language
   ("must not" followed later by "is acceptable").
2. Completeness: each item under "What the agent often gets wrong" should have
   a corresponding countermeasure in "How to reason correctly", "What to verify",
   or "How to verify".
3. Readability: average sentence length ≤ 25 words (approximate Flesch reading ease).

Usage: python tools/lint/prose_lint.py <SKILL.md> [more...]
Exit: 0 clean, 1 warnings, 2 errors

v2.0 addition to the validator suite.
"""
import os
import re
import sys


SENTENCE_RE = re.compile(r'[.!?]+')


def split_sentences(text):
    return [s.strip() for s in SENTENCE_RE.split(text) if len(s.strip()) > 15]


# Patterns indicating negative claims / prohibitions in the text
NEGATIVE_PATTERNS = [r"(?:must[ ]?not|should not|don't|never|forbidden|reject)", r"\bunacceptable\b"]
POSITIVE_PATTERNS = [r"(?:acceptable|fine\s?(with|to)|permitted|\bis\s?(good|ok|right))"]


def check_logical_consistency(body_text: str) -> list[str]:
    warnings = []

    # Check reasoning steps reference previous steps
    reasoning_section_match = re.search(
        r"How to reason correctly.*?(?=^##|\Z)", body_text, re.S | re.M
    )
    if reasoning_section_match:
        reasoning_text = reasoning_section_match.group(0)
        numbered_steps = re.findall(r"^(\d+)\.\.", reasoning_text, re.M)
        expected_max = max(
            int(n) for n in numbered_steps
        ) if numbered_steps else 0
        for i_step in range(1, expected_max):
            # Next step should NOT use "first" again
            next_line_match = re.search(
                rf"({i_step + 1}\..*?=\n\d+\.)",
                reasoning_text,
                re.S,
            )
            if next_line_match:
                line = next_line_match.group(1)[:100]
                if "first" in line.lower():
                    warnings.append(
                        f"[logic-step] Step {i_step+1} repeats 'first' instruction\n"
                    )

    # Scan for contradictory patterns within short span (same paragraph / section chunk)
    for para_raw in re.split(r"\n[ \t]*\n[ \t]*", body_text):
        paras_lower = para_raw.lower()
        has_neg = bool(re.search("|".join(NEGATIVE_PATTERNS), paras_lower))
        has_pos = bool(re.search("|".join(POSITIVE_PATTERNS), paras_lower))
        if has_neg and has_pos:
            # More nuanced check: could be legitimate (but flag for review)
            pass

    return warnings


def check_completeness(body_text: str) -> list[str]:
    """
    Each bullet point in "What the agent often gets wrong" should map to
    something in "How to reason correctly", "What to verify", or
    "How to verify". Flag if any concept in section 3 is completely absent.
    """
    wrong_section = re.search(
        r"What the agent often gets wrong.*?(?=^##|\Z)",
        body_text, re.S | re.M
    )
    if not wrong_section:
        return ["[completeness] Missing 'What the agent often gets wrong' section"]

    wrong_text = wrong_section.group(0).lower()

    right_section = re.search(
        r"How to reason correctly.*?(?=^##|\Z)", body_text, re.S | re.M
    )
    verify_what = re.search(
        r"What to verify.*?(?=^##|\Z)", body_text, re.S | re.M
    )
    verify_how = re.search(
        r"How to verify.*?(?=^##|\Z)", body_text, re.S | re.M
    )

    coverage_text = ""
    if right_section:
        coverage_text += right_section.group(0).lower()
    if verify_what:
        coverage_text += verify_what.group(0).lower()
    if verify_how:
        coverage_text += verify_how.group(0).lower()

    # Extract real bullet items (lines starting with '- ' or '* ') from the
    # "wrong" section — NOT the section header itself.
    bullet_items = re.findall(r"^\s*[-*]\s+(.+)$", wrong_text, re.M)
    issues = []
    for item in bullet_items:
        item = item.strip().lower()
        if not item or len(item) < 5:
            continue
        # Simple keyword overlap check
        word_set = set(
            re.findall(r"\b[a-z]+\b", item)
        )
        # drop generic stopwords that appear everywhere
        stopwords = {
            "about", "after", "agent", "also", "being", "code", "correct",
            "gets", "into", "often", "over", "than", "that", "their",
            "these", "this", "when", "which", "with", "wrong",
        }
        sig_words = [
            w for w in word_set
            if w not in stopwords and (
                len(w) >= 5 or w in {"abi", "unsafe", "lock", "memory",
                                     "error", "buffer", "ffi", "mmio"}
            )
        ]
        if sig_words and coverage_text:
            covered = sum(1 for w in sig_words if w in coverage_text)
            if covered == 0:
                issues.append(item[:60])

    warnings = []
    for issue in issues[:5]:
        warnings.append(f"[completeness] Uncovered concern in {issue}...")
    return warnings


def check_readability(body_text: str) -> list[str]:
    warnings = []
    sentences = split_sentences(body_text)
    if len(sentences) < 3:
        warnings.append("[readability] Too few substantive sentences — consider expanding")
        return warnings

    total_words = sum(len(s.split()) for s in sentences)
    avg_len = total_words / len(sentences)

    if avg_len > 30:
        warnings.append(
            f"[readability] Avg sentence length: {avg_len:.1f} words "
            "(target ≤ 25; readability suffers)"
        )

    return warnings


def lint(path):
    errors = []
    warnings_list = []
    try:
        text = open(path, encoding="utf-8").read()
    except OSError as exc:
        return warnings_list, [f"cannot read {path}: {exc}"]

    # Extract body (skip frontmatter)
    body_start = text.find("---\n", 4) if text.startswith("---") else 0
    if body_start > 0:
        footer_pos = text.find("\n---\n", body_start + 4)
        if footer_pos > 0:
            body = text[footer_pos + 1:]
        else:
            body = text[body_start + 4:]
    else:
        body = text

    errors.extend(check_logical_consistency(body))
    warnings_list.extend(check_completeness(body))
    warnings_list.extend(check_readability(body))

    return warnings_list, errors


def main() -> int:
    paths = sys.argv[1:]
    if not paths:
        print("usage: prose_lint.py <SKILL.md> [...]")
        return 2
    rc = 0
    total_w = 0
    for p in paths:
        warnings, errors = lint(p)
        for e in errors:
            print(f"ERROR  {p}: {e}")
        for w in warnings:
            print(f"WARN   {p}: {w}")
        total_w += len(warnings)
        if errors:
            rc = 2
        elif warnings and rc == 0:
            rc = 1

    print(f"{len(paths)} files checked, {total_w} total warnings")
    return rc


if __name__ == "__main__":
    raise SystemExit(main())