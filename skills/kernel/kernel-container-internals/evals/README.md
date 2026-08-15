# Evaluation — kernel-container-internals

Skill: `skills/kernel/kernel-container-internals`.
Toolchain status: RESEARCHED. No Linux host, no systemd, no runc, no
unshare/nsenter/capsh on this Windows machine. Commands are exact and
documented but were NOT run. Fact status is marked in
`references/container-internals.md`.

## Synthetic evals (researched — expected behavior documented, not executed)

| Case | Fixture | Expected on Linux | Command |
|---|---|---|---|
| cgroup/negative | `bad/cgroup_v1_knobs.sh` | ENOENT: v1 paths absent on v2 | `cat /sys/fs/cgroup/memory/memory.limit_in_bytes` |
| ns/negative | `bad/unshare_lowercase_c.sh` | error: unrecognized option 'c' | `unshare -c true` |
| overlay/negative | `bad/overlay_whiteout_semantics.sh` | mount needs workdir; whiteout is 0/0 char dev | `mount -t overlay -o lowerdir=...` |
| seccomp/negative | `bad/seccomp_full_sandbox.sh` | review: seccomp filters syscalls only | `grep Seccomp /proc/self/status` |
| caps/negative | `bad/userns_root_equivalence.sh` | review: userns root != host root | `unshare -Ur id` |
| ns/positive | `good/ns_probe.sh` | different nsfs inodes for unshare -m | `readlink /proc/self/ns/mnt` |
| cgroup/positive | `good/cgroup_v2_probe.sh` | v2 controllers + memory.current readable | `cat /sys/fs/cgroup/memory.current` |
| overlay/positive | `good/overlay_mount_check.sh` | finds real overlay mounts | `findmnt -t overlay` |
| seccomp/positive | `good/seccomp_caps_probe.sh` | Seccomp/Cap* lines from status | `grep -E "^(Seccomp|CapEff):" /proc/self/status` |

## Verified facts (ACTUAL on this host)

None — no Linux kernel reachable. KNOWN-from-source facts (references/):
namespace identity via nsfs inodes; v2 unified hierarchy with
`cgroup.subtree_control`; v2 memory knobs (`memory.max`/`memory.high`);
overlayfs whiteout = char 0/0; seccomp = syscall filter; runc runs bundles.
Status: UNVERIFIED by execution.

## False-positive evals (researched)

- v2-native files (`memory.max`, `memory.events`, `pids.max`,
  `cgroup.subtree_control`) must pass.
- "CAP_SYS_ADMIN in the init user namespace" (distinguished from userns)
  must pass.
- Statements that describe seccomp as one layer among several must pass.

## Adversarial evals (researched)

- Partial correctness: a fixture with correct namespace names but a wrong
  inheritance rule (children sharing a NEW mount ns after exec) must be
  caught by the nsfs-inode test.
- A memory-limit answer that says "reserves memory" must be caught by the
  charge-on-fault semantics.

## Historical evals

- The userns-root-equivalence myth and the v1-knob-on-v2 error are documented
  failure classes; reproduced as fixtures rather than fetched from a
  historical corpus (UNVERIFIED against upstream history).

## Target toolchains (absent, documented)

- Linux host with util-linux (`unshare`, `nsenter`), systemd (`systemd-run`),
  libcap (`capsh`), and runc: not present. Planned elevation: WSL or a QEMU
  guest, then run the good fixtures and record output.
