const std = @import("std");
const expectEqual = std.testing.expectEqual;

var flag: u32 = 0;

fn worker() void {
    @atomicStore(u32, &flag, 1, .release);
}

test "spawn and join" {
    const t = try std.Thread.spawn(.{}, worker, .{});
    t.join();
    try expectEqual(@as(u32, 1), @atomicLoad(u32, &flag, .acquire));
}

threadlocal var tls_value: u32 = 0;

fn tlsWorker(delta: u32) void {
    tls_value += delta;
}

test "threadlocal is per-thread" {
    tls_value = 100;
    const t = try std.Thread.spawn(.{}, tlsWorker, .{1});
    t.join();
    try expectEqual(@as(u32, 1), tls_value); // main's TLS unchanged
}
