# intentionally incorrect: v1-style knobs do not exist on the unified cgroup
# v2 hierarchy. /sys/fs/cgroup/memory/ and /sys/fs/cgroup/cpu/ are v1 mounts;
# on a v2 system these paths are absent and the writes/reads fail or hit a
# different controller. The correct v2 files are memory.max / cpu.max under
# the cgroup directory.
cat /sys/fs/cgroup/memory/memory.limit_in_bytes
cat /sys/fs/cgroup/cpu/cpu.shares
