# Evaluation — reproducible-builds-firmware

## Verified facts (host, recorded 2026-08-20)

Host: Windows, gcc 16.1.0 (MSYS2/MinGW), Python 3.11.9.
Command: `python examples/tools/repro_check.py` (exit 0) and
`python examples/good/repro_build.py` (exit 0). Real output:

```
[OK FAIL] A timestamp __DATE__/__TIME__ (bad): sha256 87d4feba2831b362 vs 7249fb555d0bbe52 (expected FAIL)
[OK PASS] B SOURCE_DATE_EPOCH recipe (good): sha256 485e3e2f4a3b2efa (expected PASS)
[OK FAIL] C path leak, no -ffile-prefix-map (bad): sha256 11ede0cbc6544ebb vs 29b9049c9351a754 (expected FAIL)
[OK PASS] C2 path leak, with -ffile-prefix-map (good): sha256 44ceea5337cbfca0 (expected PASS)
[OK FAIL] D gzip default mtime (bad): sha256 708be54bba54bd7c vs c6d43a9a27eb0a70 (expected FAIL)
[OK PASS] D2 gzip mtime=0 (good, CLI: gzip -n): sha256 5da703a2a65fd9d7 (expected PASS)
overall: PASS (all checks behave as documented)
```

Full sha256 of the good-recipe build (both directories, 2 s apart):

```
485e3e2f4a3b2efaafba3e402164e718dbfc41b653b066fc6275d32a34c1bc36
```

Full sha256 of gzip mtime=0 (both runs):

```
5da703a2a65fd9d750fc7e338f539a71322531b6d4bff68998d1cbb5a9eaba1e
```

Host findings worth noting:

- On MinGW/PE the linker embeds a `TimeDateStamp` in the PE header; without
  `-Wl,--no-insert-timestamp` two identical-code builds still differ after
  all other timestamps are pinned. This was observed empirically (two
  differing sha256 before the flag, identical after).
- Python 3.11 `gzip.compress` defaults to `mtime=None` (current time), so the
  header differs between runs; `mtime=0` makes it byte-identical. CLI
  equivalent on target hosts: `gzip -n`.

## Synthetic evals

- **Claim check**: SKILL.md states `__DATE__`/`__TIME__` break reproducibility.
  Synthetic probe: compile a file with both macros twice one second apart,
  compare sha256. Covered by check A.
- **Claim check**: prefix maps scrub absolute paths. Synthetic probe: compile
  `__FILE__` + `-g` from two different absolute dirs with and without
  `-ffile-prefix-map`. Covered by checks C/C2.
- **Claim check**: compression embeds mtime. Synthetic probe: two gzip runs
  with default and pinned mtime. Covered by checks D/D2.
- **Recipe check**: the full recipe (SOURCE_DATE_EPOCH + prefix map +
  --no-insert-timestamp) makes two fresh-dir builds byte-identical. Covered
  by check B.

## False-positive evals

- **Same-second compile**: two compiles of `nonrepro_time.c` within the same
  second can match because `__TIME__` has 1 s granularity. repro_check.py
  inserts a 2 s gap so the bad case is proven non-reproducible; do not
  mistake a same-second match for a reproducible build.
- **Idempotence vs reproducibility**: `make`/`ninja` re-building and printing
  "nothing to be done" proves idempotence, not byte reproducibility. The
  only valid evidence is two *clean* builds compared byte-wise.
- **Same dir, same second**: building twice in the same directory can hide
  path leaks (relative `__FILE__`). repro_check.py builds from two different
  absolute directories to expose the leak.
- **Build-id stability**: a stable GNU build-id after the recipe is expected
  (it is a hash of deterministic inputs); do not flag it as randomness, but
  do flag any timestamp/nanosecond-derived build-id.

## Historical evals

- **XZ Utils backdoor (CVE-2024-3094, 2024)**: a maintainer's machine was
  used to backdoor a tarball. Reproducible builds plus an SBOM do not detect
  a malicious maintainer, but they do let downstream users verify the
  artifact matches published source and force distribution of the *source*
  (with exact versions) rather than trust of a binary — the supply-chain
  loop the skill closes. A reproduction check would have surfaced the
  build-host compromise as a divergence between claimed and actual source.
- **SolarWinds Orion (2020)**: signed-but-untraceable build process; no SBOM
  tied to the signed binary. The lesson: publish an SPDX/SBOM generated from
  the same deterministic build so the signed artifact and the bill of
  materials have matching hashes.
- **Yocto CVE class**: recipes that pin a version but not a source hash
  (SSTATE/SRCREV drift) break the source↔artifact correspondence that
  reproducible builds verify. Verification step for such fixes: rebuild the
  recipe with `INHERIT += "reproducible_build"`, confirm the SBOM references
  the SRCREV actually used, and diff two clean builds.

## Adversarial evals

- **Forge timestamps deliberately**: given `-DSOURCE_DATE_EPOCH=1600000000`,
  the built-in string must be `2020-09-13 12:26:40 UTC` — assert exact
  program output in repro_build.py (it does). A build that ignores
  SOURCE_DATE_EPOCH or falls back to `__TIME__` produces a different string
  and fails the check.
- **Move the tree between builds**: check C/C2 moves the source tree to a
  different absolute path; only the prefix-map build survives.
- **Insert a 2 s wall-clock gap**: checks A and B insert the gap; the good
  recipe must be immune, the bad case must fail.
- **Locale/TZ trick**: rebuild the same source with `LC_ALL=C TZ=UTC` vs a
  non-UTC `TZ`; sort and date output must not change the final artifact.
  This is a target-side adversarial eval for Yocto/Buildroot CI.
- **Partial fix**: fix only timestamps but leave debug paths unscrubbed —
  expected FAIL; this is the "fixes only time, misses path leak" trap in the
  SKILL.md.

## Verification commands (target — Yocto/Buildroot)

Yocto:

```
# clean build 1
INHERIT += "reproducible_build"
bitbake <image>

# clean build 2 in a separate build dir, same SRCREV
sha256sum tmp/deploy/images/<machine>/*.tar.gz tmp/deploy/images/<machine>/*.bin

# SBOM from the same build
IMAGE_CLASSES += "create-spdx"
SPDX_INHERIT = "recipe-spdx"
ls tmp/deploy/spdx/*

# diff any mismatch
diffoscope a.tar.gz b.tar.gz
```

Buildroot:

```
make BR2_REPRODUCIBLE=y O=/tmp/br1 defconfig && make BR2_REPRODUCIBLE=y O=/tmp/br1
make BR2_REPRODUCIBLE=y O=/tmp/br2 defconfig && make BR2_REPRODUCIBLE=y O=/tmp/br2
sha256sum /tmp/br1/images/*  /tmp/br2/images/*
diffoscope /tmp/br1/images/<image> /tmp/br2/images/<image>
```

These toolchains are documented (researched), not executed on this host.

## Scoring

Each demo check (A, B, C, C2, D, D2) earns 1 point when the observed status
matches the documented expectation: 6/6 = PASS (all reproducible-behavior
claims host-verified). Claim-level scores:

- Timestamps (SOURCE_DATE_EPOCH, __DATE__/__TIME__): 3/3 host-verified.
- Path leaks (-ffile-prefix-map): 2/2 host-verified.
- Compression (gzip mtime): 2/2 host-verified.
- Link timestamp (--no-insert-timestamp): 1/1 host-verified (observed
  difference without it, identity with it).
- Yocto/Buildroot mechanisms and SPDX: documented from primary sources
  (researched), target-verification commands provided; not executed here.

Stability: `researched` — the underlying mechanisms are host-verified with
real gcc/Python runs; the Yocto/Buildroot class-level behavior is primary-
source documented with runnable target verification steps.
