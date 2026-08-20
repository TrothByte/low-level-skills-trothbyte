// GOOD pattern — TARGET-ONLY. Requires Linux with kernel BTF
// (CONFIG_DEBUG_INFO_BTF), clang with the BPF backend, and libbpf/bpftool.
// NOT host-compilable on Windows: vmlinux.h and bpf/bpf_core_read.h exist only
// on a Linux build host. This file documents the portable pattern; the
// host-runnable proof of the relocation mechanics is
// examples/good/core_reloc_model.py.
//
// Target build (Linux):
//   bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h
//   clang -target bpf -g -O2 -c portable_read.c -o portable_read.o
//   bpftool prog load -d portable_read.o /sys/fs/bpf/portable_read
//   # -d prints the relocations; expect nonzero resolved offsets.
//
// The -g flag is REQUIRED: without it clang emits no BTF debug info and libbpf
// has nothing to relocate against (load error: "CO-RE relocation failed").

#include <vmlinux.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>

SEC("kprobe/do_exit")
int good_portable_read(struct pt_regs *ctx)
{
    struct task_struct *task = (struct task_struct *)bpf_get_current_task();
    unsigned long rss_count = 0;

    // BPF_CORE_READ expands to a chain of bpf_core_read() calls, each of which
    // carries a __builtin_preserve_access_index relocation for the accessed
    // field. libbpf resolves task_struct.mm, mm_struct.rss_stat and
    // rss_stat.count against the target kernel's vmlinux BTF at load time, so
    // this one binary runs on every kernel where the field path exists.
    rss_count = BPF_CORE_READ(task, mm, rss_stat, count);

    return rss_count;
}

SEC("kprobe/do_exit")
int good_guarded_read(struct pt_regs *ctx)
{
    struct task_struct *task = (struct task_struct *)bpf_get_current_task();
    unsigned long rss_count = 0;

    // rss_stat.count does not exist on every kernel version (it was renamed /
    // restructured in some). bpf_core_field_exists() is resolved at load time;
    // when the field is missing the whole guarded branch is rewritten out, so
    // the program loads and degrades gracefully instead of failing to load.
    if (bpf_core_field_exists(task->mm->rss_stat.count))
        rss_count = BPF_CORE_READ(task, mm, rss_stat, count);

    return rss_count;
}

char LICENSE[] SEC("license") = "GPL";
