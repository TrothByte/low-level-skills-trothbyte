# intentionally incorrect: util-linux unshare uses -C (capital) for the
# cgroup namespace; lowercase -c is not a valid option. The agent claimed the
# letter from the CLONE_NEW* bit names, not from the tool's interface.
unshare -c true
