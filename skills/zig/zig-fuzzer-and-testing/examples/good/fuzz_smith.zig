const std = @import("std");

fn fuzzTest(_: void, smith: *std.testing.Smith) !void {
    var sum: u64 = 0;
    var count: u64 = 0;
    while (!smith.eosWeightedSimple(7, 1)) {
        sum += smith.value(u8);
        count += 1;
    }
    try std.testing.expect(sum != 1234);
    try std.testing.expect(count <= 100_000);
}

fn fuzzRange(_: void, smith: *std.testing.Smith) !void {
    const small = smith.valueRangeAtMost(u32, 100);
    if (small > 100) return error.BadValue;
}
