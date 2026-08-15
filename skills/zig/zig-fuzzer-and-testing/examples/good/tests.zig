const std = @import("std");
const builtin = @import("builtin");

fn add(a: i32, b: i32) i32 {
    return a + b;
}

test "expectEqual" {
    try std.testing.expectEqual(@as(i32, 42), add(41, 1));
}

test "expectError" {
    try std.testing.expectError(error.NotFound, lookup());
}

fn lookup() anyerror!u32 {
    return error.NotFound;
}

test "skip platform-specific" {
    if (builtin.target.os.tag != .linux) return error.SkipZigTest;
    try std.testing.expectEqual(@as(usize, 0), builtin.target.cpu.arch == .x86_64);
}

test "is_test is true in test builds" {
    try std.testing.expect(builtin.is_test);
}

test "no leaks with the testing allocator" {
    const gpa = std.testing.allocator;
    var list: std.ArrayList(u32) = .empty;
    defer list.deinit(gpa);
    try list.append(gpa, 1);
    try std.testing.expectEqual(@as(usize, 1), list.items.len);
}
