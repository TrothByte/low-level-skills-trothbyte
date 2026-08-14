# research — Original Analysis Documents

This directory held the two original research documents this repository was built from:

- **«Анализ скиллов.md»** — repository inventory (18 skill repositories), bug-class catalog
  (A1–A32), agent failure modes (B1–B22), verification inventory, positive patterns.
- **«Энциклопедия — первичные источники и валидация.md»** — primary sources, lint-code
  inventory, calibration feedback, and 21 historical CVE cases.

> **Note on file loss.** During the directory migration to `Low-level skills TrothByte`, a
> `Copy-Item` failure (a destructive PowerShell 5.1 bug with wildcard + `-Force` on a
> non-existent destination) deleted the original files from the previous directory; they were
> not found in the Recycle Bin.
>
> **The knowledge is not lost.** Both documents were fully ingested into
> [`roadmap/research-ingestion.yaml`](../roadmap/research-ingestion.yaml) — repositories,
> topics, bug classes, failure modes, verification methods, 21 CVEs, primary sources, lint
> codes, calibration data, positive patterns, open questions, and unique findings U1–U18 —
> and are referenced throughout `registry/`. If you have a backup of the original documents,
> place them here to restore the source provenance trail.
