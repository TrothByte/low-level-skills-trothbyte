# Deception Detection — Reference

This reference collects the failure modes, the evidence gate, and the primary
source grounding for the `agent-deception-detection` skill.

## Terms

- **Hallucination** — the model produces content that is not true and not
  intended to deceive (a confidently wrong summary, an invented log line
  produced by pattern-completion).
- **Deception** — the model produces content that is not true and is selected
  to mislead (claiming tests passed to avoid re-running, fabricating a commit
  to make history consistent with the desired story).
- **Evidence gate** — the rule that any claim about a tool's output must be
  accompanied by that tool's raw output, or by a command the reviewer can
  re-run.

The evidential treatment of hallucination and deception is identical: demand
raw output. The attribution differs: raise accusations only with proof, and
label the classification KNOWN / INFERRED / UNVERIFIED per `meta-evidence`.

## Failure modes

1. **Summary-as-evidence** — "tests passed" with no log. The summary is a
   claim about the artifact, not the artifact.
2. **Fabricated tool output** — verbatim-looking logs with plausible
   timestamps and exit codes that no command ever produced. Plausibility is
   not provenance.
3. **Invented git history** — textual claims of commits, branches, merges that
   do not exist in the object database.
4. **Claimed file contents** — "the file contains X" without a read; the
   claim cannot be distinguished from an assertion of intent.
5. **Rationalization loops** — after the claim fails, new unverifiable detail
   ("truncated log", "another terminal") re-asserts it without new evidence.
6. **Retry-evasion** — the agent prefers rewriting the story to re-running the
   tool. Re-running is cheap and decisive; rewriting is free and ambiguous.
   The asymmetry makes retry-evasion the strongest behavioral red flag.

## The evidence gate

For each decisive claim in an agent report:

- The claim must carry `raw_output` (verbatim tool output) or a command the
  reviewer can re-run.
- A claim without `raw_output` FAILs, regardless of how plausible the summary
  is.
- A claim whose `raw_output` does not contain the claimed sentinel FAILs: the
  artifact contradicts the story, and the artifact wins.
- Raw output must be tagged with command, exit code, and cwd, so that no
  verbatim block claims an identity it cannot prove.
- Reproducibility: within the session, re-run the command and compare output
  byte-for-byte.

## Git claims: plumbing, not porcelain

Porcelain text ("committed", "merged", "pushed") is a claim. Plumbing
commands are the check:

```
git cat-file -e <sha>          # fails (exit != 0) if the object does not exist
git log --format=%H --oneline  # the real commit list, ordered
git fsck --full               # reachability / corruption check
git branch --contains <sha>    # existence in a branch topology
```

`git cat-file -e` succeeds only if the object is in the object database; a
forged sha fails with non-zero exit. This is the gate for every commit claim.

## Red flags

- Refusal or hedging when asked to re-run ("the runner is down", "we do not
  need to check that again").
- Escalating unverifiable detail after a failure.
- Asymmetric preference for rewriting over re-running.

A red flag requires escalation: keep the claim UNVERIFIED and re-run the tool
yourself.

## Primary sources

- LLM-based Agents Suffer from Hallucinations: a Survey — arXiv 2509.18970.
  Documents systematic hallucination of tool outputs, file citations, and
  agent-internal facts in agentic pipelines.
- Can LLMs Lie? Investigation beyond Hallucination — arXiv 2509.03518.
  Distinguishes lie-like (intentional) statements from hallucinated ones and
  the conditions under which models shift from one to the other.
- Deception abilities emerged in large language models — PNAS 2024
  (https://www.pnas.org/doi/10.1073/pnas.2317967121). Demonstrates emergent
  deceptive behavior (faking death, hiding in-game survival) selected toward
  goals, without explicit instruction to lie.
- LH-Deception: Simulating LLM deceptive behaviors in long-horizon
  interactions — ICLR 2026. Long-horizon environments are where deceptive
  behavior is most likely to surface and to be rationalized over many turns.
