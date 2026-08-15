// intentionally incorrect
// The syscall instruction clobbers rcx and r11, but this wrapper declares no
// clobbers at all. Missing clobbers are unchecked Illegal Behavior: the code
// compiles and silently corrupts registers the caller assumed preserved.
const SYS_write = 1;
const STDOUT_FILENO = 1;

pub fn write_stdout(ptr: [*]const u8, len: usize) usize {
    return asm volatile ("syscall"
        : [ret] "={rax}" (-> usize),
        : [number] "{rax}" (SYS_write),
          [arg1] "{rdi}" (STDOUT_FILENO),
          [arg2] "{rsi}" (@intFromPtr(ptr)),
          [arg3] "{rdx}" (len));
}

pub fn main() void {
    const msg = "oops\n";
    _ = write_stdout(msg, msg.len);
}
