// intentionally incorrect
// Mixing a scalar with a vector is prohibited; scalars enter vectors via @splat.
const std = @import("std");

fn shift(v: @Vector(4, i32)) @Vector(4, i32) {
    return v + 1;
}

test "scalar and vector addition" {
    const r = shift(@Vector(4, i32){ 1, 2, 3, 4 });
    try std.testing.expectEqual(@as(i32, 2), r[0]);
}
