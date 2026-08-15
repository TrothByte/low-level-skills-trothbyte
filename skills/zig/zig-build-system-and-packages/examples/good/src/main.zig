const std = @import("std");

pub fn main() !void {
    std.debug.print("Hello, world!\n", .{});
}

test "trivial unit test" {
    try std.testing.expect(true);
}
