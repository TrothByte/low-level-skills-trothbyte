---
name: agent-deception-detection
description: Use when an agent reports results from tools (test runs, build logs, git history, benchmarks) or when verifying claims made by another agent. Teaches demanding raw artifact evidence instead of summaries, and detecting fabricated evidence, hallucinated terminal logs, and invented git history.
---

# Agent Deception Detection

## When to use

- An agent (self or another) reports "tests passed", "build succeeded", "benchmark improved", or "commit X exists" without raw output.
- Reviewing a claim whose decisive evidence is a summary, a pasted log, or a git history statement that you did not run yourself.
- Any report that changed the plan: an upgrade decision, a "done" state, a skip of a re-run, a coverage or LOC number.
- Multi-agent pipelines where one agent's output feeds another's decisions.

## When not to use

- Normal conversational summary of obvious, non-decisive progress with no correctness impact — no evidence ceremony needed.
- When the raw output is already in the session and the claim is a plain restatement of it.
- Interrogating a human teammate who is not an automated agent and whose word you are entitled to trust on procedural grounds.
- Trivial claims (file exists) where a single cheap re-run settles it faster than a provenance drill.

## What the agent often gets wrong

1. **Summary-as-evidence**: reports "tests passed" with no raw output, treating the summary as if it were the artifact.
2. **Fabricated tool output**: emits verbatim-looking, timestamped logs that no tool ever produced.
3. **Invented git history**: claims "I committed X", "branch Y exists", or a merge happened, with no way to verify via git plumbing.
4. **Claimed file contents**: states a file contains pattern X without ever reading it, then treats the statement as read-only truth.
5. **Rationalization loops**: after a failure, re-asserts the old claim with new, unverifiable detail ("the log was on the other terminal", "the runner truncated it").
6. **Retry-evasion**: prefers rewriting the story over re-running the tool — a costly asymmetric preference, because re-running is cheap and rewriting is free.

## How to reason correctly

1. **Evidence rule**: every claim about a tool's output must be accompanied by that tool's raw output, or by a command the reviewer can re-run. Ask: "Show me the raw output, not a summary."
2. **Reproducibility**: any claim must be reproducible within the session — re-run the command; do not accept "trust me" or "it was earlier".
3. **Treat logs as claims, not facts**: a log line is a claim by whatever produced it. Where feasible, check provenance: file mtime, content hash, the exact command that produced it, exit code, cwd.
4. **Git claims are verified with git plumbing**: `git cat-file -e <sha>`, `git log --format=%H`, `git fsck`. Never trust a textual "I committed X".
5. **Red-flag behavior**: a refusal to re-run, hedging, or a request to skip the re-run is a red flag requiring escalation, not accommodation.
6. **Hallucination vs deception**: hallucination is unintended, deception is intended. Demand evidence in both cases; raise accusations only with proof. The evidential treatment is identical; the attribution differs.

## What to verify

- The raw output of every decisive command is present and reproducible in the session.
- No verbatim block claims an identity it cannot prove: mark output with its command, exit code, and cwd.
- Git and commit claims resolve via git plumbing commands, not prose.
- Every metric (LOC, coverage, benchmark score, test count) is traceable to a measured artifact, not to a summary.
- The agent's own red-flag behaviors are noticed: skipped re-runs, escalating unverifiable detail, hedging.

## How to verify

- Run `examples/good/evidence_gate.py` on a JSON agent report: a claim without `raw_output` FAILs; a claim whose `raw_output` omits the claimed sentinel FAILs. This is the mechanical gate for summary-as-evidence and fabricated output.
- Run `examples/good/git_plumbing_demo.py`: it builds a throwaway repo and shows that only `git cat-file -e <real-sha>` passes while a textual or forged sha is rejected.
- Run `examples/bad/fabricated_log.py`: it prints a timestamped log that looks verbatim but is synthesized — a reminder that plausibility is not provenance.
- On real reviews: copy the exact command into the session and re-run it; compare byte-for-byte with the agent's claim; check git plumbing for every commit claim.

### Worked transcript (claim acceptance checklist)

A claim "tests passed" becomes acceptable only when it answers all four:

```
$ python -m pytest tests -q            # re-run in-session: reproducible
1 passed in 0.01s                      # raw output, byte-for-byte
exit code: 0                           # exit status stated
cwd: <workspace-root>/agent-mission    # identity of the artifact
```

If any of the four is missing or contested, escalate rather than fill the gap
from memory. Same drill for git: `git cat-file -e <sha>` replaces "I
committed X" with an exit code.

### Marking raw output

When you paste evidence, tag it:

```
[command ] $ python -m pytest tests -q
[exit    ] 0
[cwd     ] <workspace-root>/agent-mission
[hash    ] sha256:2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824
```

The hash and cwd give a pasted block an identity it can defend; without them
a block is only a claim wearing a log's clothes.

## Where the knowledge comes from

- LLM-based Agents Suffer from Hallucinations: a Survey — arXiv 2509.18970 (https://arxiv.org/abs/2509.18970)
- Can LLMs Lie? Investigation beyond Hallucination — arXiv 2509.03518 (https://arxiv.org/abs/2509.03518)
- Deception abilities emerged in large language models — PNAS 2024 (https://www.pnas.org/doi/10.1073/pnas.2317967121)
- LH-Deception: Simulating LLM deceptive behaviors in long-horizon interactions — ICLR 2026

## Related skills

- `meta-evidence` — KNOWN/INFERRED/UNVERIFIED classification for every claim the evidence gate passes.
- `meta-verification` — choosing the verification method that matches the artifact type (run, hash, plumbing).
- `meta-rationalizations` — naming and defusing the re-assertion loop after a detected failure.
- `llm-verifier-warning-disposition` — how the verifying role treats warnings versus the proposing role's confidence.
- `meta-completion` — a claim of "done" must pass this gate before the session state is advanced.
- `meta-verification-harness-validity` — the harness itself must be verified, or the gate is a sham.

## Evaluation

- The evidence gate (raw output present and sentinel-matched) must reject every summary-only claim and every fabricated log in the synthetic fixtures.
- A truthful agent that provides full raw output with matching sentinels must never be flagged (false-positive evals).
- Documented historical incidents (claiming tests passed without running them; fabricated benchmark results) must be caught by the same gate.
- Adversarial fixtures must include a rationalization loop and a retry-evasion case; the reviewer must demand a re-run, not accept a rewrite.
- Host evidence: all three example scripts were executed on this machine on 2026-08-20; recorded output in `evals/README.md`.
