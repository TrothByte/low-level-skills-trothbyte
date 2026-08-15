// intentionally incorrect
// On the error path BOTH the defer and the errdefer free buf: a double free
// exactly when cleanup matters most. Pick one cleanup mechanism per resource.
const std = @import("std");

fn make(allocator: std.mem.Allocator) ![]u8 {
    const buf = try allocator.alloc(u8, 16);
    defer allocator.free(buf);
    errdefer allocator.free(buf);

    var sum: usize = 0;
    for (buf) |b| sum += b;
    if (sum == 0) return error.EmptyBuffer;

    return buf;
}

test "double cleanup on the error path" {
    const gpa = std.testing.allocator;
    try std.testing.expectError(error.EmptyBuffer, make(gpa));
}
