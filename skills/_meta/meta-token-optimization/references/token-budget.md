# Meta-Token-Optimization: Token Budget & Progressive Disclosure — Reference Rules

Knowledge layer for `meta-token-optimization`. RULE → WHY AI GETS IT WRONG →
CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE → VERIFICATION → SOURCE.

## 1. Activation cost is SKILL.md only; the gate is hard

- **RULE**: activation cost = metadata (name+description frontmatter) + body
  of SKILL.md, measured by `tools/tokens/token_measure.py`. The v2.0 gate is
  ≤ 2000 tokens and is enforced by `validate.py` as an ERROR — a skill that
  exceeds it fails CI. References and examples load lazily and do not count
  toward activation.
- **WHY AI GETS IT WRONG**: agents optimize total skill size (references +
  examples) and leave SKILL.md bloated, or treat 2000 as a soft target. The
  hard gate makes the SKILL.md alone the critical file; everything else is
  cheap.
- **CORRECT REASONING**: the token that matters is the one the agent reads on
  every invocation. Every sentence in SKILL.md must justify being read at
  activation; anything the agent needs only occasionally moves out.
- **EXAMPLE** (bad): a SKILL.md that embeds the full instruction-set table
  "because it is important" — it consumes most of the budget at activation
  and repeats the reference.
- **COUNTEREXAMPLE** (good): the same SKILL.md says "encoding tables:
  references/encodings.md" and keeps only the trigger rules and verification
  commands in the body.
- **VERIFICATION**: `python tools/tokens/token_measure.py --check 2000
  <skill-dir>` — exit 1 on violation; `python tools/validate.py` runs it.
- **SOURCE**: tools/tokens/token_measure.py (docstring + --check mode);
  docs/architecture.md.

## 2. tiktoken vs heuristic: measure, then trust the tool's verdict

- **RULE**: `token_measure.py` uses tiktoken (cl100k_base) when installed and
  falls back to a chars/3.5 + words/1.3 blended heuristic. The verdict that
  matters is the tool's gate result — comparing raw "word counts" between
  skills is meaningless across languages.
- **WHY AI GETS IT WRONG**: agents estimate tokens by word count and conclude
  "under budget" without running the tool, or quote heuristic numbers as if
  they were exact encodings.
- **CORRECT REASONING**: the tool is the arbiter; the heuristic is a
  fallback. If tiktoken is unavailable the heuristic still gates (same
  threshold), but results should be labeled as estimates.
- **EXAMPLE** (bad): an agent counts words in a Russian-heavy SKILL.md and
  claims 1500 tokens, while the tool's encoding (or heuristic) says 1900 —
  close to the gate and unstable across edits.
- **COUNTEREXAMPLE** (good): the agent runs the tool, sees 1900, moves a table
  to references, re-runs, sees 1650 — recorded.
- **VERIFICATION**: run the two commands in the skill's "How to verify";
  the second re-run after the edit is the regression check.
- **SOURCE**: tools/tokens/token_measure.py (--check 2000 gate); arxiv-2607-00107 (rigorous claims require reproducible measurement).

## 3. Progressive disclosure: SKILL.md routes, references deliver

- **RULE**: SKILL.md contains only what must be true on every activation
  (triggers, failure modes, reasoning steps, verification commands, source
  pointers). Depth (tables, per-tool detail, worked examples, terminology)
  lives in `references/<topic>.md` and is loaded on demand.
- **WHY AI GETS IT WRONG**: agents flatten all knowledge into one file
  because "it's complete" — the completeness praise should go to the
  references, not to a bloated SKILL.md. Also, agents remove sections from
  SKILL.md without creating the reference, silently losing knowledge.
- **CORRECT REASONING**: every move is a relocation, not a deletion: the
  reference file must exist, be linked, and be token-consistent. A skill is
  complete when the SKILL.md is compact AND the references are thorough.
- **EXAMPLE** (bad): a SKILL.md that deletes the instruction-latency table
  from the body with no reference file — knowledge lost, gate "passed".
- **COUNTEREXAMPLE** (good): the table moves to `references/encodings.md`
  verbatim and the SKILL.md adds one routing line to it.
- **VERIFICATION**: diff the removed section vs the new reference; re-run
  `token_measure.py` and confirm both the gate and the reference's existence.
- **SOURCE**: docs/architecture.md (knowledge layer design).

## 4. Duplication is the hidden bloat

- **RULE**: knowledge that already exists elsewhere (this skill's references,
  another skill's references, the registries) must be referenced, not
  re-embedded. Duplicate text double-counts tokens and rots independently —
  the copy in the skill drifts from the authoritative copy.
- **WHY AI GETS IT WRONG**: agents re-explain common concepts (UB, atomics,
  ABI) in every skill "for completeness", and re-cite the same provenance in
  every reference instead of reusing claim ids.
- **CORRECT REASONING**: a cross-link (`meta-*` or domain skill) is cheaper
  than a paragraph and stays correct. Claims go in `registry/claims.yaml`
  once; references cite the id.
- **EXAMPLE** (bad): two skills each embedding the same 40-line ABI table —
  80 redundant tokens plus two copies to keep in sync.
- **COUNTEREXAMPLE** (good): one skill holds the table; the other cross-links
  it via `Related skills`.
- **VERIFICATION**: grep the skill dir for text also present in other skills'
  references; count as duplication when verbatim overlap exceeds a paragraph.
- **SOURCE**: docs/architecture.md; registry/claims.yaml.

## 5. Structural gates that cannot be token-traded away

- **RULE**: SKILL.md body ≤ 250 lines and frontmatter description ≤ 50 words
  are separate hard errors (`skill_lint.py`), independent of the token gate.
  Token optimization must never trade these structural requirements for
  budget — e.g. removing a required section to save tokens.
- **WHY AI GETS IT WRONG**: agents treat all constraints as interchangeable
  and "fix" a token violation by deleting a required section, turning a
  gate-pass into a lint-fail.
- **CORRECT REASONING**: satisfy all three constraints at once: compact but
  complete body, ≤ 50-word description, ≤ 2000 tokens. When they conflict,
  the answer is relocating depth to references, not deleting content.
- **EXAMPLE** (bad): deleting the "What to verify" section to cut 60 tokens —
  now skill_lint fails on a missing section.
- **COUNTEREXAMPLE** (good): condensing the section to one line per bullet
  and moving the expanded checklist to references.
- **VERIFICATION**: `python tools/lint/skill_lint.py <SKILL.md>` then
  `python tools/tokens/token_measure.py --check 2000 <dir>` — both must exit 0.
- **SOURCE**: tools/lint/skill_lint.py (MAX_LINES, MAX_DESC_WORDS).
