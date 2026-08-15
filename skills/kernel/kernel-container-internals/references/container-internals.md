# Kernel Container Internals — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE(bad) →
COUNTEREXAMPLE(good) → VERIFICATION → SOURCE. Source ids refer to
registry/sources.yaml. Status tags: KNOWN = verifiable in the cited docs/
spec; INFERRED = from secondary sources, confirm on target kernel.

## 1. A namespace is an inode identity, not a feature flag

- **RULE**: each process lives in exactly one namespace per type (mnt, pid,
  net, uts, ipc, user, cgroup, time). The identities are visible as
  `/proc/self/ns/*` symlinks into nsfs (e.g. `mnt:[4026531840]`). `unshare`
  creates new ones; `nsenter` switches into existing ones; two processes in
  the same namespace share the same inode number.
- **WHY AI GETS IT WRONG**: models describe namespaces as boolean options on
  the process and lose the "one per type, inherited at fork/exec" identity
  model, so they get inheritance and `nsenter` semantics wrong.
- **CORRECT REASONING**: read the inode: `readlink /proc/self/ns/mnt`.
  Equality of inode == same namespace. A child inherits all namespaces of
  the parent at clone(2) time; exec changes nothing.
- **EXAMPLE** (bad): `unshare -c true` (lowercase flag invented; the real
  flag is `-C` for cgroup).
- **COUNTEREXAMPLE** (good):
  ```sh
  readlink /proc/self/ns/mnt
  unshare -m sh -c 'readlink /proc/self/ns/mnt'
  nsenter -t $$ -m -p -u -- true
  ```
- **VERIFICATION**: compare the two `mnt:` inode values. Researched — no
  Linux host here, commands documented not run.
- **SOURCE**: linux-namespaces (man namespaces(7), man unshare(1)).

## 2. User namespaces: uid 0 there is not host root

- **RULE**: a user namespace maps uids/gids; uid 0 inside a new userns has
  full *capabilities within that namespace* but not in the init userns.
  CAP_SYS_ADMIN in a non-init userns does not grant host mounts, host
  devices, or loading modules. Unprivileged creation is controlled by
  `kernel.unprivileged_userns_clone`/`kernel.apparmor_restrict_unprivileged_userns`.
- **WHY AI GETS IT WRONG**: "root in the container is host root" — the most
  common container safety misconception. Agents also equate CAP_SYS_ADMIN
  with total kernel authority.
- **CORRECT REASONING**: capabilities are scoped to the owning user
  namespace; the kernel checks that the *capability* and the *userns* are
  the right pair for each privileged operation. Root-in-userns is a
  distinct subject from host root.
- **EXAMPLE** (bad): "uid 0 in userns == host root for everything".
- **COUNTEREXAMPLE** (good): run `unshare -Ur`, `id`, then try a host
  operation (mount a real device) and watch it fail with EPERM.
- **VERIFICATION**: `unshare -Ur id; unshare -Ur sh -c 'mount -t proc proc /mnt'`.
  Researched — not run here.
- **SOURCE**: linux-namespaces (user_namespaces(7)); kernel-source
  (kernel/user_namespace.c).

## 3. cgroups v2 is one unified hierarchy with explicit controller enables

- **RULE**: cgroup v2 mounts a single tree at `/sys/fs/cgroup`; controllers
  (cpu, memory, io, pids, etc.) are enabled per subtree by writing to
  `cgroup.subtree_control` (e.g. `+memory +cpu`). There are no v1-style
  `memory.limit_in_bytes`/`cpu.shares` files, no per-controller mount trees,
  no `tasks` file (use `cgroup.procs`/`cgroup.threads`), and a child cgroup
  with internal processes cannot host further children ("no internal process
  constraint").
- **WHY AI GETS IT WRONG**: v1 dominates training data; models emit
  `memory.limit_in_bytes` or `cpu.shares` for a v2 system, or forget the
  subtree_control enable step, or place limits in the root cgroup where
  controllers are not enabled.
- **CORRECT REASONING**: first verify v2: `/sys/fs/cgroup/cgroup.controllers`
  exists. Then enable: `echo "+cpu +memory" >
  /sys/fs/cgroup/cgroup.subtree_control`. Knobs are `memory.max`,
  `memory.high`, `memory.current`, `memory.events` (oom/oom_kill), `cpu.max`,
  `cpu.weight`, `io.max`, `pids.max`.
- **EXAMPLE** (bad): `cat /sys/fs/cgroup/memory/memory.limit_in_bytes`.
- **COUNTEREXAMPLE** (good):
  ```sh
  [ -f /sys/fs/cgroup/cgroup.controllers ] && cat /sys/fs/cgroup/memory.current
  systemd-run --scope -p MemoryMax=16M sleep 0.2
  ```
- **VERIFICATION**: `cat /proc/self/cgroup`; `findmnt /sys/fs/cgroup`.
  Researched — not run here.
- **SOURCE**: cgroup-v2 (docs.kernel.org cgroup-v2.rst).

## 4. cgroup v2 memory controller semantics: high vs max, OOM events

- **RULE**: `memory.high` throttles above the value (reclaim, not kill);
  `memory.max` is the hard limit (OOM kill, counted in `memory.events` as
  `oom_kill`/`max`); `memory.swap.max` limits swap. Setting only `memory.max`
  does not reserve pages — the controller is not a partition.
- **WHY AI GETS IT WRONG**: agents treat `memory.high` as a kill threshold
  and `memory.max` as pre-reserved memory.
- **CORRECT REASONING**: describe the lifecycle: charge on page fault → over
  `high` triggers reclaim (throttle) → over `max` triggers OOM in the
  cgroup, sibling tasks keep running. Watch `memory.events` counters.
- **EXAMPLE** (bad): "memory.high kills the process at 100%".
- **COUNTEREXAMPLE** (good): "memory.max triggers oom_kill; memory.high only
  throttles; watch memory.events for the kill counter".
- **VERIFICATION**: `echo 1G > /sys/fs/cgroup/.../memory.max` then a
  stress-allocator; `cat .../memory.events`. Researched — not run here.
- **SOURCE**: cgroup-v2 (memory section).

## 5. OverlayFS: layers, copy-up, whiteouts

- **RULE**: overlayfs stacks a `lowerdir` (read-only base), an `upperdir`
  (writable), and a `merged` view; `workdir` is required for copy-up. Reads
  merge layers; writes copy-up the file to upperdir; deletions create
  whiteouts (char devices 0/0), not empty files.
- **WHY AI GETS IT WRONG**: models claim writes go to the lower layer or that
  whiteouts are empty regular files; they also forget `workdir` is mandatory
  on current kernels.
- **CORRECT REASONING**: name the three directories and the copy-up trigger:
  a write to a lower file first copies it to upperdir, then modifies the
  copy. Whiteout as 0/0 char dev explains why `ls` shows nothing but the
  lower entry is gone.
- **EXAMPLE** (bad): "deleting a lower file leaves an empty file in merged".
- **COUNTEREXAMPLE** (good): whiteout = char 0/0 in upperdir; the merged view
  hides the lower entry.
- **VERIFICATION**: `findmnt -t overlay -o TARGET,OPTIONS`; inspect
  `mount | grep overlay`. Researched — not run here.
- **SOURCE**: kernel-docs-fs (overlayfs.rst); oci-runtime-spec (rootfs
  layers context).

## 6. OCI runtime spec: config.json drives runc; spec is the contract

- **RULE**: an OCI bundle is `config.json` + a rootfs. The runtime (runc)
  reads `ociVersion`, `process.args/env/capabilities`, `root.path`,
  `mounts[]`, and `linux.namespaces[]`/`linux.seccomp`/`linux.rlimits`.
  `runc spec` generates a template; `runc run`/`create`+`start` execute it.
  Spec compliance is what makes an image portable across runtimes.
- **WHY AI GETS IT WRONG**: agents describe "the container runtime executes
  the Dockerfile" (it does not — the image's config is extracted to an OCI
  bundle) and invent runtime config keys.
- **CORRECT REASONING**: name the bundle files and the config.json sections
  that map to kernel primitives: namespaces → clone(2) flags, seccomp →
  filter, capabilities → capability sets, rlimits → setrlimit.
- **EXAMPLE** (bad): "runc builds the image from a Dockerfile".
- **COUNTEREXAMPLE** (good): "runc only runs a bundle; an image is unpacked
  into rootfs + config.json before runc sees it".
- **VERIFICATION**: `runc spec; cat config.json | head`; `runc run` a test
  bundle. Researched — not run here.
- **SOURCE**: oci-runtime-spec (config.md, runtime.md).

## 7. seccomp filters syscalls; it is not a sandbox

- **RULE**: seccomp filters syscalls via BPF (SECCOMP_MODE_FILTER /
  SECCOMP_RET_*), defaulting to `SECCOMP_RET_ERRNO`/`KILL` depending on
  policy. It blocks *syscalls*, not filesystem/network semantics — an
  allowed `read()` on a leaked fd is still a read. `Seccomp:` in
  `/proc/self/status` and `Seccomp_filters:` report the state.
- **WHY AI GETS IT WRONG**: "seccomp fully sandboxes the process" overstates;
  agents also claim filters can deny memory access or that setting a filter
  twice is a complete containment.
- **CORRECT REASONING**: seccomp is one layer (syscall allow/deny). File
  access, networking, and memory are governed by the syscalls that stay
  allowed (open, connect, mmap...) plus LSM/caps. Without CONFIG_SECCOMP,
  PR_SET_SECCOMP fails.
- **EXAMPLE** (bad): "the process is now fully sandboxed by seccomp".
- **COUNTEREXAMPLE** (good): "the filter denies clone/execve; open() stays
  allowed, so file access through fd 0 still works".
- **VERIFICATION**: `grep Seccomp /proc/self/status`; run a filter that kills
  execve and observe EPERM/ENOSYS. Researched — not run here.
- **SOURCE**: linux-namespaces (seccomp(2), prctl(2)); oci-runtime-spec
  (linux.seccomp section).

## Quick reference table

| Claim | Correct fact | Status |
|---|---|---|
| Namespace identity | nsfs inode via `/proc/self/ns/*`, inherited at clone | KNOWN |
| userns root | caps scoped to owning userns; not host root | KNOWN |
| cgroup v2 tree | unified, controllers via subtree_control | KNOWN |
| v2 memory | high=throttle, max=oom_kill, events file | KNOWN |
| v1 files | `memory.limit_in_bytes`/`cpu.shares` absent on v2 | KNOWN |
| OverlayFS | lower+upper+work; copy-up; whiteout=0/0 char dev | KNOWN |
| OCI | runc runs a bundle, not a Dockerfile | KNOWN |
| seccomp | syscall filter, not a full sandbox | KNOWN |
| unshare flags | `-m -u -i -n -p -U -C -T` (capital C) | KNOWN |
