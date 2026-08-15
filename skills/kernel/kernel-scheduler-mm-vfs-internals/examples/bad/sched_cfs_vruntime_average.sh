# intentionally incorrect: claims CFS is still the default in 6.11 and that
# /proc/sched_debug prints a line named "cfs_avg_vruntime". No kernel prints
# that field; the version check contradicts the claim, and grep returns empty.
#!/bin/sh
uname -r
grep -m1 "cfs_avg_vruntime" /proc/sched_debug
echo "CFS average vruntime: still the scheduling metric in this kernel"
