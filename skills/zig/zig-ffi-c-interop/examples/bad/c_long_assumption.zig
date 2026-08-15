// intentionally incorrect
// c_long is 32-bit on Windows (LLP64) and 64-bit on Linux/macOS. Assuming
// i64 silently breaks the ABI on Windows. Use c_long (or time_t via translate-c).
const std = @import("std");

extern "c" fn time(p: ?*i64) i64;

test "c_long is target-dependent" {
    const t = time(null);
    _ = t;
    if (@sizeOf(c_long) != 8) @compileError("expected 64-bit c_long on this target");
}
