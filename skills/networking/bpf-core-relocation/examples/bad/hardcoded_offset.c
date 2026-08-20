// BAD pattern — TARGET-ONLY. Hardcoded struct offset copied from the
// disassembly / BTF of one specific kernel version (here offset 0x10 for
// task_struct.mm). The program compiles and loads, but on any other kernel
// version where task_struct layout differs it SILENTLY reads the wrong field —
// no error, just garbage (or a fault if the bogus pointer is then dereferenced
// directly). This is the exact failure the CO-RE machinery exists to prevent.
// NOT host-compilable on Windows; shown for review. The host-runnable model of
// this failure is examples/bad/reloc_misuse.py (checker exit 1).
//
// Target build (Linux, only to see it load and misbehave):
//   clang -target bpf -g -O2 -c hardcoded_offset.c -o hardcoded_offset.o
//   bpftool prog load hardcoded_offset.o /sys/fs/bpf/hardcoded_offset

#include <vmlinux.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>

// offsetof(struct task_struct, mm) as measured on the kernel this program was
// tuned against. No relocation, no guard: baked-in at compile time.
#define HARDCODED_TASK_MM 0x10

SEC("kprobe/do_exit")
int bad_hardcoded_offset(struct pt_regs *ctx)
{
    struct task_struct *task = (struct task_struct *)bpf_get_current_task();
    struct mm_struct *mm;

    // WRONG: cast the task pointer to an address and read the u64 at the
    // hardcoded offset. On the tuned kernel this is task_struct.mm; on a
    // kernel where the field moved (or a new field now occupies 0x10) this is
    // some other field, silently read as a pointer. It is not relocated and
    // bpf_core_field_exists cannot guard it because there is no relocation.
    mm = *(struct mm_struct **)((char *)task + HARDCODED_TASK_MM);

    // Even this direct deref is doubly wrong: mm may point at garbage, and the
    // deref bypasses bpf_core_read, so in probe context it can fault.
    return mm->map_count;
}

// The whole point: clang -target bpf accepts this, bpftool loads it, and the
// verifier sees a plain pointer read — nothing is wrong "for this kernel".
// Correctness across kernels requires CO-RE relocations (see
// examples/good/portable_read.c).

char LICENSE[] SEC("license") = "GPL";
