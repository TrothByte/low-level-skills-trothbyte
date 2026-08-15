const std = @import("std");
const expectEqual = std.testing.expectEqual;

test "element-wise vector add" {
    const a = @Vector(4, i32){ 1, 2, 3, 4 };
    const b = @Vector(4, i32){ 5, 6, 7, 8 };
    const c = a + b;
    try expectEqual(@as(i32, 6), c[0]);
    try expectEqual(@as(i32, 12), c[3]);
}

test "splat broadcasts a scalar" {
    const v: @Vector(4, i32) = @splat(7);
    try expectEqual(@as(i32, 7), v[2]);
}

test "vector to array coercion for runtime iteration" {
    const v = @Vector(4, i32){ 1, 2, 3, 4 };
    const arr: [4]i32 = v;
    var total: i32 = 0;
    for (arr) |e| total += e;
    try expectEqual(@as(i32, 10), total);
}
