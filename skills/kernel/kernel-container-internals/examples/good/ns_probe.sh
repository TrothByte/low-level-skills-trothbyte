#!/bin/sh
# Method: prove namespace claims from /proc/self/ns/* inode numbers, never
# from memory of CLONE_* bit values.
set -u
echo "== my namespaces =="
for n in mnt pid net uts ipc user cgroup; do
    printf "%s: " "$n"
    readlink "/proc/self/ns/$n" || echo "not present (older kernel)"
done
echo "== inside a new mount namespace (different inode) =="
unshare -m sh -c 'readlink /proc/self/ns/mnt'
echo "== nsenter into the caller's namespaces =="
nsenter -t $$ -m -p -u -- true && echo "nsenter ok"
