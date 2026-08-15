# intentionally incorrect: there is no /proc/sys/kernel/kgdb sysctl. kgdb is
# entered via the kgdboc parameter + sysrq 'g' (or cmdline kgdbwait). This
# write fails with ENOENT — the agent guessed a knob that does not exist.
echo 1 > /proc/sys/kernel/kgdb
