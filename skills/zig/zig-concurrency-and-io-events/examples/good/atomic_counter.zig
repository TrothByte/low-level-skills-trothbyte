const std = @import("std");
const expectEqual = std.testing.expectEqual;

const N_THREADS = 4;
const N_INCREMENTS = 1000;

var counter: u32 = 0;

fn increment() void {
    var i: u32 = 0;
    while (i < N_INCREMENTS) : (i += 1) {
        _ = @atomicRmw(u32, &counter, .Add, 1, .monotonic);
    }
}

test "atomic counter across threads" {
    var threads: [N_THREADS]std.Thread = undefined;
    for (&threads) |*t| {
        t.* = try std.Thread.spawn(.{}, increment, .{});
    }
    for (&threads) |*t| {
        t.join();
    }
    try expectEqual(@as(u32, N_THREADS * N_INCREMENTS), counter);
}
