#!/bin/sh
# overlayfs probe: find real overlay mounts and their options. Do not assume
# lowerdir/upperdir paths from memory.
set -u
findmnt -t overlay -o TARGET,OPTIONS -n 2>/dev/null || mount | grep overlay || echo "no overlay mounts"
