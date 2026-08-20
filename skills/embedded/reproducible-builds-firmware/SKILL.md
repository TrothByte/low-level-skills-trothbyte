---
name: reproducible-builds-firmware
description: Use when building firmware that must be bit-for-bit reproducible — Yocto, Buildroot, SOURCE_DATE_EPOCH, SBOM/SPDX, or verifying two identical source trees produce identical artifacts. Teaches timestamp and path normalization for reproducible embedded builds.
---

# Reproducible Firmware Builds

## When to use

- A firmware release must be bit-for-bit reproducible: two identical source
  trees built with the same toolchain must produce byte-identical artifacts.
- Working in Yocto (`INHERIT += "reproducible_build"`) or Buildroot
  (`BR2_REPRODUCIBLE`) and debugging why outputs still differ.
- Setting or auditing `SOURCE_DATE_EPOCH` so timestamps embedded by gcc, ld,
  ar, gzip, cpio, and zip do not vary between builds.
- Scrubbing build paths from debug info and `__FILE__` with
  `-ffile-prefix-map` / `-fdebug-prefix-map` / `-fmacro-prefix-map`.
- Generating an SBOM/SPDX document from the same deterministic build so the
  published binary can be verified against published source (supply-chain
  trust).
- Auditing whether a claimed "reproducible" pipeline actually is — the
  standard check is building twice in clean trees and diffing the artifacts.

## When not to use

- First-time board bring-up where no release artifact exists yet — use
  `embedded-board-bringup-peripheral-init` or `hardware-register-bringup`.
- Debugging a build failure caused by stale objects after an interrupted
  build — use `build-process-signal-and-state-safety`.
- ABI/symbol drift between toolchain versions — use
  `build-toolchain-version-drift`; reproducibility is a different property
  (same toolchain, same bytes).
- Signing/verification infrastructure design — use `secure-boot-chain`;
  reproducibility supports signing but does not replace it.
- Stack-protector/hardening-flag selection for the firmware — use
  `binary-hardening-flags`.

## What the agent often gets wrong

1. Claiming a build is "deterministic" because `make` re-outputs the same
   result on one machine — that only proves idempotence, not that a fresh
   checkout builds to identical bytes.
2. Using `__DATE__`/`__TIME__` as a "version string" and calling the build
   reproducible. The version string is exactly what breaks reproducibility;
   the macros must be replaced with a `SOURCE_DATE_EPOCH`-derived value.
3. Fixing only timestamps and missing build-path leaks: debug info still
   embeds absolute source paths, so two builds from different directories
   differ even when every time field is pinned.
4. Running plain `gzip`/`zip` without `-n`/`--no-name`/`-X`, and forgetting
   that `ar` and `ld` also embed timestamps (PE/COFF header
   `TimeDateStamp`, ELF `.comment`/build-id input hashes).
5. Forgetting that locale and `TZ` affect `sort`, `date`, and tar header
   ordering, so the same commands produce different bytes on differently
   configured machines.
6. Not verifying with the standard method: two clean builds + `diffoscope`.
   An unverified "reproducible" claim is a claim, not a result.
7. Confusing reproducibility with hermeticity. `SOURCE_DATE_EPOCH` fixes
   time; it does not pin the toolchain. Reproducibility needs both a fixed
   time base and a pinned toolchain (containers, Yocto SDK).

## How to reason correctly

1. Set `SOURCE_DATE_EPOCH` globally from the source revision
   (`git log -1 --format=%ct`) and export it to every build step so all
   tools that honor it (gcc, ld, ar, gzip, zip, javac, CPython) use the same
   time base.
2. Eliminate every timestamp and path: no `__DATE__`/`__TIME__` (derive the
   value from `SOURCE_DATE_EPOCH`), `-ffile-prefix-map` on every compile and
   link, `gzip -n`/`zip -X`, and `ld --no-insert-timestamp` (binutils 2.40+).
3. Fix ordering: deterministic link order from explicit object lists or
   sorted wildcards — never rely on shell-glob or make-job ordering.
4. Prove it: build twice in fresh directories and diff the artifacts with
   `diffoscope` until zero differences; keep that two-build comparison as a
   CI gate, not an afterthought.
5. On Yocto/Buildroot, enable the project's documented mechanism
   (`reproducible_build` class / `BR2_REPRODUCIBLE`) and verify with the
   project's own tests before chasing custom fixes.
6. Emit the SBOM/SPDX from the deterministic build so hashes recorded in the
   SBOM match the artifact actually released.

## What to verify

- `SOURCE_DATE_EPOCH` is set and actually consumed: two clean builds hash
  identically (compare sha256 of the final artifacts).
- No `__DATE__`/`__TIME__` remains in the tree (grep) unless overridden by a
  `SOURCE_DATE_EPOCH`-derived macro.
- `-ffile-prefix-map` (or `-fdebug-prefix-map` + `-fmacro-prefix-map`)
  appears in every compiler invocation, not only the leaf compile step.
- Two fresh builds from different directories produce byte-identical
  artifacts; this is the whole point, so it must be the gate.
- The SBOM/SPDX document is generated from the same deterministic build and
  references the artifact hashes.

## How to verify

Run the host-runnable demonstrations in `examples/` with the bundled helper:

```
python examples/tools/repro_check.py
```

The helper builds the bad and good cases with the local gcc, hashes the
outputs, and reports each check as PASS (reproducible) or FAIL (not
reproducible):

- `bad/nonrepro_time.c` (uses `__DATE__`/`__TIME__`) compiled twice two
  seconds apart must produce different sha256 (FAIL — as expected for bad).
- `good/fixed_timestamp.c` compiled twice in different directories with
  `-DSOURCE_DATE_EPOCH=1600000000`, `-ffile-prefix-map`, and
  `-Wl,--no-insert-timestamp` must produce identical sha256 (PASS).
- `bad/pathleak.c` compiled from two different absolute directories without
  a prefix map must differ (FAIL), and must match with
  `-ffile-prefix-map=ABS=src` (PASS).
- Compression: Python `gzip.compress` (mtime defaults to now) must differ
  between runs (FAIL); `mtime=0` must match (PASS) — the portable analogue
  of `gzip -n`.

For target toolchains (Yocto/Buildroot), verify with the exact recipes in
`references/reproducible-firmware.md` and the standard two-clean-build +
`diffoscope` procedure; those toolchains are documented, not run on this
host.

## Where the knowledge comes from

- Reproducible Builds project (https://reproducible-builds.org/), SOURCE_DATE_EPOCH spec (https://reproducible-builds.org/specs/source-date-epoch/)
- Yocto Project reproducible builds docs (https://docs.yoctoproject.org/dev-manual/reproducible-builds.html)
- Buildroot reproducible builds docs (https://buildroot.org/downloads/manual/manual.html#reproducible-builds)
- SPDX in Yocto (https://docs.yoctoproject.org/dev-manual/sbom.html)
- gcc debug-prefix-map docs (https://gcc.gnu.org/onlinedocs/gcc/Developer-Options.html)

## Related skills

- `embedded-ota-bootloader-safety`
- `build-toolchain-version-drift`
- `binary-hardening-flags`
- `secure-boot-chain`
- `build-process-signal-and-state-safety`
- `embedded-linker-script`

## Evaluation

See `evals/README.md` for host-verified results (recorded 2026-08-20),
synthetic and false-positive eval design, historical supply-chain incidents
(XZ backdoor class), adversarial evals, target-toolchain verification
commands, and scoring. The host demonstrations (gcc 16.1.0 MinGW, Python
3.11.9) are real: they were run once on this machine and their sha256 values
are recorded there.
