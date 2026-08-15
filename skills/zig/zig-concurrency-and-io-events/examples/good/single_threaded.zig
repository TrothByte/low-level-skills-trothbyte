const std = @import("std");
const expectEqual = std.testing.expectEqual;

test "single-threaded code is correct as-is" {
    var sum: u32 = 0;
    for (0..10) |i| sum += @intCast(i);
    try expectEqual(@as(u32, 45), sum);
}

fn sequentialSum(n: u32) u32 {
    var total: u32 = 0;
    var i: u32 = 0;
    while (i < n) : (i += 1) total += i;
    return total;
}

test "no threads needed for this workload" {
    try expectEqual(@as(u32, 45), sequentialSum(10));
}
