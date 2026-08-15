// intentionally incorrect
// Per-lane integer overflow is Illegal Behavior: in Debug/ReleaseSafe this
// panics. @reduce(.Add) is wrapping, so switching to ReleaseFast silently
// wraps instead of detecting the overflow — exactly the NEON counter-overflow
// failure class.
const std = @import("std");

fn sum_bytes(v: @Vector(4, u8)) u8 {
    return @reduce(.Add, v);
}

test "overflowing u8 sum" {
    const v = @Vector(4, u8){ 200, 200, 200, 200 };
    try std.testing.expectEqual(@as(u8, 160), sum_bytes(v));
}
