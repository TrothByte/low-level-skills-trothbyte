// intentionally incorrect
// 0.16.0 forbids runtime vector indexes. Indexing with a loop variable is a
// compile error; coerce to an array first.
const std = @import("std");

fn sum(v: @Vector(4, i32)) i32 {
    var total: i32 = 0;
    for (0..4) |i| {
        total += v[i];
    }
    return total;
}

test "runtime index into a vector" {
    try std.testing.expectEqual(@as(i32, 10), sum(@Vector(4, i32){ 1, 2, 3, 4 }));
}
