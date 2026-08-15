---
name: kernel-container-internals
description: Use when writing or reviewing container-adjacent kernel claims — namespaces, cgroups v2, overlayfs, OCI/runc bundles, seccomp, and capability semantics. Prevents v1-era cgroup knobs, userns-root myths, and seccomp-as-sandbox overstatements from passing as facts.
---

# Kernel Container Internals

## When to use

- Explaining or reviewing how containers isolate: namespaces (mnt/pid/net/
  uts/ipc/user/cgroup), cgroups v2 controllers, overlayfs rootfs, OCI/runc
  bundles, seccomp profiles, capability sets.
- Debugging container problems: limits not applied, "root inside == root
  outside" confusion, overlay writes failing, seccomp killing the process.
- Reading `/proc/self/ns/*`, `/proc/self/status` capability/seccomp lines,
  `/sys/fs/cgroup/**` knobs.

## When not to use

- Docker/Podman CLI workflows (build/push/run) — that is user-space tooling.
- Kernel scheduler/MM/VFS behavior — use `kernel-scheduler-mm-vfs-internals`.
- Kernel instrumentation/debug tooling — use
  `kernel-debugging-ftrace-kprobes-kdump`.

## What the agent often gets wrong

- Writing v1 cgroup knobs (`memory.limit_in_bytes`, `cpu.shares`) on a v2
  system, or forgetting `cgroup.subtree_control` enables.
- Claiming "root in the container is host root" or that CAP_SYS_ADMIN in a
  userns grants host-level power.
- Treating `memory.high` as a kill threshold and `memory.max` as reserved RAM.
- Claiming seccomp is a full sandbox; it filters syscalls only.
- Wrong overlayfs semantics: whiteouts as empty files, writes to the lower
  layer, missing `workdir`.
- Inventing tool flags (`unshare -c` lowercase for cgroup namespace).
- Saying "runc builds the image from a Dockerfile" — runc runs OCI bundles.

## How to reason correctly

1. For every isolation claim, name the kernel primitive behind it: clone(2)
   flags → namespaces, cgroup controller files → limits, mount layers →
   overlayfs, BPF filters → seccomp.
2. Verify hierarchy state from files, not memory: `readlink /proc/self/ns/mnt`,
   `cat /proc/self/cgroup`, `cat /sys/fs/cgroup/cgroup.controllers`.
3. For cgroup v2, remember: enable controllers in subtree_control, then use
   v2-only files (`memory.max`, `memory.high`, `cpu.max`, `pids.max`).
4. Scope every capability/userns claim: what does uid 0 here actually hold,
   and in which user namespace?
5. Treat seccomp as a syscall filter, never a sandbox; layer it with caps and
   LSMs when describing containment.

## What to verify

- cgroup v2 mount and controller enable state before quoting a knob.
- The namespace inode of the process you describe (`readlink /proc/self/ns/*`).
- `Seccomp:`/`Seccomp_filters:` lines and `Cap*` masks in `/proc/self/status`.
- Overlay mount options (`findmnt -t overlay -o TARGET,OPTIONS`).
- The OCI bundle layout (config.json + rootfs) before claiming runtime behavior.

## How to verify

```
readlink /proc/self/ns/mnt
unshare -m sh -c 'readlink /proc/self/ns/mnt'
nsenter -t $$ -m -p -u -- true
cat /proc/self/cgroup
cat /sys/fs/cgroup/cgroup.controllers
echo "+cpu +memory" > /sys/fs/cgroup/cgroup.subtree_control
cat /sys/fs/cgroup/memory.current
systemd-run --scope -p MemoryMax=16M sleep 0.2
findmnt -t overlay -o TARGET,OPTIONS
grep -E "^(Seccomp|Seccomp_filters|CapBnd|CapEff):" /proc/self/status
capsh --print
runc spec && cat config.json
```

On this host (no Linux, no systemd, no runc) all of the above are researched
and documented, not executed.

## Where the knowledge comes from

- `linux-namespaces` — namespaces(7), user_namespaces(7), unshare(1),
  nsenter(1), seccomp(2), prctl(2) man pages.
- `cgroup-v2` — Documentation/admin-guide/cgroup-v2.rst (unified hierarchy,
  controller files, memory semantics).
- `oci-runtime-spec` — config.md, runtime.md (bundle layout, process,
  linux.namespaces, linux.seccomp, capabilities).

## Related skills

- `kernel-scheduler-mm-vfs-internals` — the scheduler/MM machinery cgroups
  control.
- `kernel-debugging-ftrace-kprobes-kdump` — instrumenting container runtime
  behavior inside a guest.
- `kernel-uaccess-safety`, `kernel-atomic-context` — kernel-side rules that
  bound what a container escape can actually reach.

## Evaluation

- Synthetic: bad fixtures must be caught — v1 knobs on v2, `unshare -c`,
  whiteout semantics, seccomp-as-sandbox, userns-root equivalence.
- False-positive: v2-native knobs (`memory.max`, `memory.events`,
  subtree_control) and userns-aware statements must pass unmodified.
- Adversarial: a "partial correctness" claim — correct namespace names but a
  wrong inheritance rule, or correct caps list but host-root equivalence —
  must be flagged.
- Historical: no curated container-CVE corpus is registered; the userns-root
  myth is reproduced as a fixture instead (UNVERIFIED against upstream
  history).
- Researched gap: `unshare`/`nsenter`/`systemd-run`/`runc` are not runnable
  on this host; exact commands are documented for a Linux host.
