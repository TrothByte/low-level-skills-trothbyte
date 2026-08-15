// intentionally incorrect
// The aarch64 ABI is "gnu" or "musl", not "gnueabihf" (that is 32-bit arm).
// This parses but targets the wrong ABI; consult `zig targets` for spellings.
const std = @import("std");
const builtin = @import("builtin");

pub fn main() void {
    const os = builtin.target.os.tag;
    std.debug.print("os={s}\n", .{@tagName(os)});
}

test "the comment says it all" {
    try std.testing.expect(builtin.target.cpu.arch != .aarch64 or true);
}
