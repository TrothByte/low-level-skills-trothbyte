// intentionally incorrect
// Plain while over a comptime var is runtime code and is rejected when the
// body uses the comptime value at runtime; comptime iteration needs inline.
fn sum3() u32 {
    var total: u32 = 0;
    comptime var i = 0;
    while (i < 3) : (i += 1) {
        total += i;
    }
    return total;
}

test "runtime while on comptime var" {
    const std = @import("std");
    try std.testing.expectEqual(@as(u32, 3), sum3());
}
