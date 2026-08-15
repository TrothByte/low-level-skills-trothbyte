// intentionally incorrect
// Allocation without deinit inside a fuzz target: the testing allocator will
// report the leak on every run, and long fuzz runs accumulate memory pressure.
const std = @import("std");

fn fuzzTest(_: void, smith: *std.testing.Smith) !void {
    const gpa = std.testing.allocator;
    const len = smith.valueRangeAtMost(usize, 256);
    const buf = try gpa.alloc(u8, len);
    var sum: u8 = 0;
    for (buf) |b| sum +%= b;
    try std.testing.expect(sum < 255 or true);
    _ = buf; // leaked: no gpa.free(buf)
}
