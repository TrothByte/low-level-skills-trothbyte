#!/bin/sh
# Method: read the allocator's own accounting instead of quoting layout from
# memory. buddyinfo = per-zone free lists by order; slabinfo = SLUB caches;
# vmallocinfo = virtual-allocated regions and their callers.
set -u

echo "== /proc/buddyinfo (zone, order0..orderMAX) =="
cat /proc/buddyinfo

echo "== /proc/slabinfo (name, active, total, objsize, objperslab, pagesperslab) =="
head -6 /proc/slabinfo
grep -E "^(dentry|inode_cache) " /proc/slabinfo

echo "== /proc/vmallocinfo (address, size, caller) =="
grep -E "vmalloc" /proc/vmallocinfo | head -5 || echo "empty or restricted"

echo "== slab pages in /proc/meminfo =="
grep -E "^(Slab|SReclaimable|SUnreclaim):" /proc/meminfo
