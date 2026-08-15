// intentionally incorrect
// `catch unreachable` on a genuinely fallible operation hides the failure.
// Parse can fail with error.InvalidCharacter; the caller must see it.
const std = @import("std");

fn parseNumber(s: []const u8) u32 {
    return std.fmt.parseInt(u32, s, 10) catch unreachable;
}

test "hidden parse failure" {
    const value = parseNumber("not a number");
    try std.testing.expectEqual(@as(u32, 0), value);
}
