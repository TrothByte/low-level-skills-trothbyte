// intentionally incorrect
// The test allocator tracks this allocation; without a matching deinit the
// default test runner reports a leak and the test command exits nonzero.
const std = @import("std");

test "leak detected by the testing allocator" {
    const gpa = std.testing.allocator;
    var list: std.ArrayList(u21) = .empty;
    try list.append(gpa, 'x');
    try std.testing.expectEqual(@as(usize, 1), list.items.len);
}
