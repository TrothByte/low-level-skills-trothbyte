const std = @import("std");
const expectEqual = std.testing.expectEqual;

test "defer runs at scope exit, LIFO" {
    var log: [8]u8 = undefined;
    var n: usize = 0;

    {
        defer log[n] = '1';
        n += 1;
        defer log[n] = '2';
        n += 1;
    }

    try expectEqual(@as(usize, 2), n);
    try expectEqual('2', log[0]);
    try expectEqual('1', log[1]);
}

test "defer inside a loop body runs per iteration" {
    var count: usize = 0;
    var i: usize = 0;
    while (i < 3) : (i += 1) {
        defer count += 1;
    }
    try expectEqual(@as(usize, 3), count);
}

fn cleanupOrder() []const u8 {
    var log: [3]u8 = undefined;
    var n: usize = 0;
    defer {
        log[n] = 'a';
        n += 1;
    }
    defer {
        log[n] = 'b';
        n += 1;
    }
    return log[0..n];
}

test "defers unwind in reverse" {
    const log = cleanupOrder();
    try expectEqual(@as(usize, 2), log.len);
    try expectEqual('b', log[0]);
    try expectEqual('a', log[1]);
}
