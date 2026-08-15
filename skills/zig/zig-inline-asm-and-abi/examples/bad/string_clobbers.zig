// intentionally incorrect
// Pre-0.15 stringly-typed clobbers. Zig 0.15.0 introduced typed clobbers
// (the langref: ": .{ .rcx = true, .r11 = true }"); string lists no longer compile.
const SYS_write = 1;
const STDOUT_FILENO = 1;

pub fn write_stdout(ptr: [*]const u8, len: usize) usize {
    return asm volatile ("syscall"
        : [ret] "={rax}" (-> usize),
        : [number] "{rax}" (SYS_write),
          [arg1] "{rdi}" (STDOUT_FILENO),
          [arg2] "{rsi}" (@intFromPtr(ptr)),
          [arg3] "{rdx}" (len)
        : "rcx", "r11");
}

pub fn main() void {
    const msg = "oops\n";
    _ = write_stdout(msg, msg.len);
}
