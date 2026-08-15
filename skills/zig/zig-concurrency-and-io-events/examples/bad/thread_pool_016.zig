// intentionally incorrect
// std.Thread.Pool was removed in Zig 0.16.0. This 0.15-era pool usage fails
// to compile on 0.16+; use std.Io.Threaded or your own pool on std.Thread.
const std = @import("std");

fn buildPool(allocator: std.mem.Allocator) !void {
    var pool: std.Thread.Pool = undefined;
    try pool.init(.{ .allocator = allocator });
    defer pool.deinit();
}

test "Thread.Pool on 0.16+" {
    try buildPool(std.testing.allocator);
}
