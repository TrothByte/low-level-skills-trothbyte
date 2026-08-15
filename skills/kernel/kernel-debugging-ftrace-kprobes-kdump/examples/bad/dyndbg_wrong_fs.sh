# intentionally incorrect: dynamic_debug control is in debugfs
# (/sys/kernel/debug/dynamic_debug/control), not tracefs. This write fails
# with ENOENT; any "pr_debug now enabled" claim after it is imaginary.
echo "module x +p" > /sys/kernel/tracing/dynamic_debug/control
