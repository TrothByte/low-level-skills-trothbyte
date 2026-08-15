// intentionally incorrect
// Data race: the shared counter is read-modify-written from multiple threads
// with no atomic or lock. Data races are Illegal Behavior. A passing test run
// proves nothing — interleavings are timing-dependent.
const std = @import("std");

const N_THREADS = 4;
const N_INCREMENTS = 1000;

var counter: u32 = 0;

fn increment() void {
    var i: u32 = 0;
    while (i < N_INCREMENTS) : (i += 1) {
        counter += 1;
    }
}

test "unsynchronized shared counter" {
    var threads: [N_THREADS]std.Thread = undefined;
    for (&threads) |*t| {
        t.* = try std.Thread.spawn(.{}, increment, .{});
    }
    for (&threads) |*t| {
        t.join();
    }
    try std.testing.expectEqual(@as(u32, N_THREADS * N_INCREMENTS), counter);
}
