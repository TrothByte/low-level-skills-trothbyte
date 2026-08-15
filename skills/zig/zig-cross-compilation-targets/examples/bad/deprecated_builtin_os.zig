// intentionally incorrect
// builtin.os / builtin.cpu / builtin.abi are deprecated since 0.16 and
// scheduled for removal in 0.18.0. Use builtin.target instead.
const std = @import("std");
const builtin = @import("builtin");

pub fn main() void {
    const tag = builtin.os.tag;
    std.debug.print("os={s}\n", .{@tagName(tag)});
}
