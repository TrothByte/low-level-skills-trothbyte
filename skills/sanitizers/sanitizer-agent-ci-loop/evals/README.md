# Evaluation — sanitizer-agent-ci-loop

Skill: `skills/sanitizers/sanitizer-agent-ci-loop`. Stability target: `evaluated`.

## Adversarial evals

- **AD-12 (fuzzer silently dies)**: a libFuzzer/AFL++ run without `-rss_limit_mb=0`/`-m none`
  under ASan. The agent must detect the silent death (no coverage growth), fix the flags,
  and refuse to report "no bugs". This is trailofbits issue #181.
- **AD-07 (sanitizer passes but race)**: ASan-clean concurrency code — agent must know ASan
  does not detect races and add TSan/Miri.

## Synthetic evals

- **easy/negative**: build step missing sanitizer flags entirely — flag it.
- **medium/negative**: sanitizer report with duplicate entries — agent must deduplicate by
  (category, file, line) before triage.
- **hard/negative**: a "clean" ASan run with no coverage evidence — agent must request proof
  the path executed.
- **positive**: correct loop (build+run+parse+dedupe+track) — must NOT be flagged.

## False-positive evals

- A run where sanitizer genuinely passed with flags and coverage recorded — must NOT be flagged.
- `-rss_limit_mb=0` in a normal (non-ASan) fuzzer — harmless, must NOT be flagged as wrong.

## Verification commands

```
clang -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -o prog prog.c && ./prog
# fuzzer: confirm corpus growth / coverage, not just exit 0
python tools/eval/sanitizer_parse.py <report.txt>   # deduped findings table
```

## Scoring

- loop presence: every change goes through build+run+parse+dedupe+track.
- issue-#181 awareness: silent fuzzer death detected and fixed.
- honest reporting: "clean" only claimed with run evidence.
