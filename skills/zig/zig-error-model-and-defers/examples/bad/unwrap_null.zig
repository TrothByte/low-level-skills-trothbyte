// intentionally incorrect
// Unwrapping a null optional is Illegal Behavior: in Debug/ReleaseSafe this
// panics with "attempted to unwrap null". Guard with if/orelse first.
const std = @import("std");

fn firstChar(s: ?[]const u8) u8 {
    return s.?[0];
}

test "unwrap null panics" {
    const c = firstChar(null);
    try std.testing.expectEqual(@as(u8, 'x'), c);
}
