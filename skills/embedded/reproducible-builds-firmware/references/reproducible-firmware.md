# Reproducible Firmware Builds — Reference

Target toolchains (Yocto, Buildroot) are documented here, not executed on this
host. Host-verified demonstrations of the underlying mechanisms (SOURCE_DATE_
EPOCH, prefix maps, PE link timestamps, gzip mtime) live in
`../examples/` and are recorded in `../evals/README.md`.

## SOURCE_DATE_EPOCH spec

The Reproducible Builds spec: the build system sets `SOURCE_DATE_EPOCH` to a
fixed integer (seconds since 1970-01-01) and exports it; tools that support it
(gcc, gzip, ld, ar, zip, javac, CPython, tar, etc.) use that value instead of
the current wall-clock time for all embedded timestamps.

Derive it from the source revision so it is stable for a given revision:

```
export SOURCE_DATE_EPOCH=$(git log -1 --format=%ct)
```

gcc honors it for `__DATE__`/`__TIME__` only when those macros are *also*
usable; the recommended pattern is to not use the macros at all and to define
a build timestamp from the macro value:

```
gcc -DSOURCE_DATE_EPOCH=$(git log -1 --format=%ct) -o app.elf app.c
```

In C, the string "2020-09-13 12:26:40 UTC" above corresponds to
`SOURCE_DATE_EPOCH=1600000000`.

## Path scrubbing (gcc)

- `-fdebug-prefix-map=OLD=NEW` — rewrite paths in debug info only.
- `-fmacro-prefix-map=OLD=NEW` — rewrite paths in `__FILE__`/`__BASE_FILE__`.
- `-ffile-prefix-map=OLD=NEW` — implies both debug and macro mapping; this is
  the flag to use at every compile+link invocation so two checkouts of the
  same revision at different absolute paths build to the same bytes.

Scrub the build root (not just the source subdir); debug paths can point at
headers anywhere under the build root.

## Link/archive timestamps

- `ld --no-insert-timestamp` (binutils 2.40+) removes the PE/COFF header
  `TimeDateStamp`. On MinGW/PE hosts this flag is what made the host
  demonstration byte-identical; without it the two identical-code builds
  differ even after every other timestamp is pinned (real result in
  `../evals/README.md`).
- `ar` embeds member mtimes; strip them with `ar -D` (deterministic mode,
  binutils 2.24+).
- The ELF `.comment`/GNU build-id is a hash of inputs — deterministic inputs
  give a deterministic build-id. Never append a random/nano-second build id.

## Compression and archives

- `gzip -n` / `--no-name` — do not store name and mtime in the gzip header.
  Python equivalent: `gzip.compress(data, mtime=0)`.
- `zip -X` — exclude extra file attributes; use a fixed timestamp option so
  the ZIP entry mtimes do not vary.
- `cpio`/tar — pass fixed mtimes (`--mtime` or `SOURCE_DATE_EPOCH`).

## Locale and timezone

`sort` order and `date`/`tar` formatting depend on `LC_ALL` and `TZ`. Pin
both in the build environment:

```
export LC_ALL=C
export TZ=UTC
```

## Yocto

Enable the reproducible builds class:

```
INHERIT += "reproducible_build"
```

The class sets `SOURCE_DATE_EPOCH` per recipe (from the recipe's git/SRCREV
metadata where available) and patches known non-reproducible tools. Verify
with the project's own test:

```
# after building
bitbake <image>
# compare two builds or run the reproducible-build test in oe-selftest
```

To emit an SPDX SBOM from the same build:

```
IMAGE_CLASSES += "create-spdx"
SPDX_INHERIT = "recipe-spdx"
```

which records exact recipe versions, sources, and licenses into SPDX files
under `tmp/deploy/spdx/`. The SBOM hashes should be generated from the
deterministic artifacts so SBOM content matches what is released.

## Buildroot

Enable the reproducible build option:

```
BR2_REPRODUCIBLE=y
```

which sets `SOURCE_DATE_EPOCH` and normalizes file permissions/ownership so
two builds of the same source configuration produce identical images.
Verify with the standard procedure: two fresh output trees (`O=` separate
dirs), then `diffoscope` the images.

## Verification procedure (the gate)

1. `git clean -fdx`, full clean build in tree 1; identical clean build in
   tree 2 (different absolute path).
2. `sha256sum` the final artifacts (fw.bin, rootfs, bootloader, SPDX docs).
3. `diffoscope` any differing artifact to identify the remaining source of
   nondeterminism; iterate until zero differences.
4. Keep the two-clean-build comparison as a CI gate on every release branch.

## Host demonstration results (2026-08-20, gcc 16.1.0 MinGW, Python 3.11.9)

Recorded outputs from `examples/tools/repro_check.py` and
`examples/good/repro_build.py` are quoted in `../evals/README.md`. The two
key numbers:

- Good recipe, two dirs, 2 s apart: sha256
  `485e3e2f4a3b2efaafba3e402164e718dbfc41b653b066fc6275d32a34c1bc36` (both).
- Path-leak fixed with `-ffile-prefix-map=ABS=src`: sha256
  `44ceea5337cbfca0...` (both); unfixed builds differ.

Known limitation: the PE/ELF headers of the host toolchain embed paths in
debug info, so `-g` builds without a prefix map differ across directories —
this is exactly the leak the skill teaches; the fix is demonstrated live.
