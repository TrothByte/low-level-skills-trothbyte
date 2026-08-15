const std = @import("std");

fn fuzzTest(_: void, input: []const u8) !void {
    var sum: u64 = 0;
    for (input) |b| {
        sum += b;
    }
    try std.testing.expect(sum != 1234);
}
