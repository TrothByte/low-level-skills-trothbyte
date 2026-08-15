const std = @import("std");
const expectEqual = std.testing.expectEqual;

fn dot4_safe(a: @Vector(4, u8), b: @Vector(4, u8)) u16 {
    const aa: @Vector(4, u16) = a;
    const bb: @Vector(4, u16) = b;
    return @reduce(.Add, aa * bb);
}

test "widen before accumulating avoids per-lane overflow" {
    const a = @Vector(4, u8){ 255, 1, 2, 3 };
    const b = @Vector(4, u8){ 1, 1, 1, 1 };
    try expectEqual(@as(u16, 255 + 1 + 2 + 3), dot4_safe(a, b));
}

test "saturating add never wraps" {
    const v: @Vector(4, u8) = .{ 250, 0, 0, 0 };
    const step = @as(@Vector(4, u8), @splat(10));
    const sat = v +| step;
    try expectEqual(@as(u8, 255), sat[0]);
}
