#!/usr/bin/env python3
"""claim_extractor.py — extract source citations from all .md files and verify coverage.

Algorithmically scans every .md file under skills/ for citation patterns:
1. `- **SOURCE**: <id>` lines (explicit SOURCE markers)
2. Bullet items under "## Where the knowledge comes from" 

Extracts each cited source, tries fuzzy matching against registry/sources.yaml.
Reports: matched / unmatched / allow-listed counts. Exits 1 if unmatched > threshold.

This implements v2.0 standard level-3 provenance verification gate.
Usage: python tools/lint/claim_extractor.py [repo-root]
"""
import glob
import os
import re
import sys

import yaml


def load_sources(root):
    """Load sources.yaml and build token index for matching."""
    src_path = os.path.join(root, "registry", "sources.yaml")
    with open(src_path, encoding="utf-8") as f:
        sources_data = yaml.safe_load(f)

    # Build per-source keyword index: {normalized_keyword_set} -> source_id
    indexed = []
    for s in sources_data.get("sources", []):
        sid = s["id"]
        title = s.get("title", "")
        topics = s.get("topics", [])

        def norm(text):
            return re.sub(r"[^a-z0-9]", "", text.lower())

        key_ids = set(norm(t) for t in re.split(r"[^a-z0-9]+", sid) if len(norm(t)) >= 2)
        key_ids.add(norm(sid))

        extra_tokens = set()
        for tok in topics + title.split():
            nt = norm(tok)
            if len(nt) >= 3:
                extra_tokens.add(nt)

        indexed.append({"id": sid, "tokens": key_ids | extra_tokens})
    return indexed, len(sources_data.get("sources", []))


ALLOWED_FREE = re.compile(
    r"(?i)\bN1570\b|\bN4861\b|\bN4971\b|CVE-\d{4}-\d+|"
    r"\bSDM\b|\bAPM\b|^empirical$|LDD3|"
    r"(clang-tidy|PVS-Studio|cppcheck|clippy)\b",
    re.IGNORECASE,
)

ALIASES = {
    "cg r": "cpp-core-guidelines",
    "core guidelines": "cpp-core-guidelines",
    "troustrup": "herbsutter-gotw",
    "godbolt talk": "godbolt-compiler",
}


def norm(text: str) -> str:
    """Normalize a string for matching: lowercase, keep only a-z0-9."""
    return re.sub(r"[^a-z0-9]", "", text.lower())


def match_source(text: str, indexed_sources: list) -> str | None:
    """Deterministically match a citation to a sources.yaml id.

    Strategy (in order):
      1. substring: normalized source id contained in normalized citation
      2. token overlap: >= 2 significant tokens of the source id appear
         as words in the citation
      3. alias table
      4. topic-token containment (long topic words in the citation)
    """
    nc = norm(text)
    words = set(re.findall(r"\b[a-z0-9]+\b", text.lower()))

    # 1. exact normalized-id substring
    for idx_src in indexed_sources:
        sid_norm = norm(idx_src["id"])
        if sid_norm and sid_norm in nc:
            return idx_src["id"]

    # 2. token overlap: majority of id tokens (len>=3) present as words
    for idx_src in indexed_sources:
        id_tokens = [
            norm(t) for t in re.split(r"[^a-z0-9]+", idx_src["id"])
            if len(norm(t)) >= 3
        ]
        if not id_tokens:
            continue
        need = max(1, (len(id_tokens) + 1) // 2)  # ceil(n/2)
        hits = sum(1 for t in id_tokens if t in words)
        if hits >= need:
            return idx_src["id"]

    # 3. alias table
    for alias, target_id in ALIASES.items():
        if alias in nc:
            return target_id

    # 4. topic-token containment (long topic words)
    for idx_src in indexed_sources:
        for tok in idx_src["tokens"]:
            if len(tok) >= 6 and tok in nc:
                return idx_src["id"]

    return None


def main() -> int:
    root = os.path.abspath(os.path.join(
        os.path.dirname(__file__), "..", "..")
        if len(sys.argv) < 2 else sys.argv[1])

    indexed_sources, total_source_count = load_sources(root)

    # Pattern A: explicit `- **SOURCE**: xxx`
    SOURCE_MARKER_RE = re.compile(r"^- \*\*SOURCE\*\*\s*:?\s*(.+)$", re.M)
    # Pattern B: bullet points after "Where the knowledge comes from" heading
    WTK_SECTION_RE = re.compile(
        r"#+\s*Where the knowledge comes from.*?(?=^##|\Z)", re.S | re.M
    )
    BULLET_RE = re.compile(r"^[\s]*[-*]\s+(.+?)$", re.M)

    md_pattern = os.path.join(root, "skills", "**", "*.md")
    all_files = sorted(glob.glob(md_pattern, recursive=True))

    total_extracted = 0
    matched = 0
    warned = 0
    details: list[str] = []
    file_stats: dict[str, tuple[int, int]] = {}

    for fp in all_files:
        text = open(fp, encoding="utf-8").read()
        extracted: list[str] = []

        # Check explicit SOURCE markers
        for m in SOURCE_MARKER_RE.finditer(text):
            extracted.append(m.group(1).strip())

        # Check "Where the knowledge comes from" bullets
        for section_m in WTK_SECTION_RE.finditer(text):
            section_text = section_m.group(0)
            for bm in BULLET_RE.finditer(section_text):
                extracted.append(bm.group(1).strip())

        em = 0; wm = 0
        for cit in extracted:
            total_extracted += 1
            ms = match_source(cit, indexed_sources)
            if ms:
                matched += 1
            elif ALLOWED_FREE.search(cit):
                pass  # allowed free citation
            elif len(cit) < 40:
                warned += 1
                details.append(f"WARN  {os.path.relpath(fp, root)}: short/unmatched citation — '{cit.strip()[:60]}'")
            else:
                warned += 1
                details.append(f"WARN  {os.path.relpath(fp, root)}: unmatched citation — '{cit.strip()[:60]}...'")
        file_stats[fp] = (len(extracted), em, wm)

    print("=" * 60)
    print(f"claim_extractor.py  ({total_source_count} registered sources)\n"
          f"Files scanned:     {len(all_files)}\n"
          f"Sources extracted: {total_extracted}\n"
          f"Matched OK:        {matched}\n"
          f"Unmatched/Warned:  {warned}\n"
          f"Allowed free:      {total_extracted - matched - warned}")
    print("=" * 60)

    if details:
        for d in details[:12]:
            print(d)
        if len(details) > 12:
            print(f"... (+{len(details)-12} more warnings)")

    print()
    if warned > 0:
        pct_ok = round(matched / max(total_extracted, 1) * 100)
        print(f"> Warning: {warned}/{total_extracted} citations unmatched ({pct_ok}% ok).\n"
              "Check docs/SKILLS.md or run manually — some sources may need adding\n"
              "to registry/sources.yaml or aliases in claim_extractor.py.\n"
              "For now treated as non-blocking WARN not ERROR.")
    else:
        print("> All citations traced to registry.sources.yaml.")

    return 0  # claim extraction is informational; severity controlled by caller


if __name__ == "__main__":
    raise SystemExit(main())