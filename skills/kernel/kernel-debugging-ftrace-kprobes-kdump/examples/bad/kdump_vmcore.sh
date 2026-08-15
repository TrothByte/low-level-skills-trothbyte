# intentionally incorrect: /proc/vmcore is present only inside a crash kernel
# booted from a crashkernel= reservation. On a normal boot the file does not
# exist and this "dump" reads nothing; the agent should first verify the
# reservation and boot state.
cat /proc/vmcore | head -c 100
