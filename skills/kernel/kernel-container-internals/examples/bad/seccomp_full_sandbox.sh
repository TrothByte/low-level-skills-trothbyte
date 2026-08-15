# intentionally incorrect: seccomp is claimed to be a complete sandbox that
# blocks filesystem access and protects against all kernel vulnerabilities.
# seccomp only filters syscalls; file access goes through allowed syscalls
# and a vulnerable-but-allowed syscall stays exploitable.
echo "process is now fully sandboxed by seccomp"
