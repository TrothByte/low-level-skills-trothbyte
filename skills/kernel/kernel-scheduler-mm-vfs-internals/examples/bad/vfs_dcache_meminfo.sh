# intentionally incorrect: claims /proc/meminfo has a "Dcache:" line for the
# dcache and that freed dentries (d_count==0) remain in the dcache hash.
# meminfo exposes Slab/SReclaimable, not per-cache lines; an unhashed dentry
# is not reachable by lookups.
#!/bin/sh
grep -E "^Dcache:" /proc/meminfo
echo "dcache shrinkable via /proc/sys/vm/drop_caches=2"
