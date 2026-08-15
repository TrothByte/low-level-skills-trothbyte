const std = @import("std");

test "no leaks with the testing allocator" {
    const gpa = std.testing.allocator;
    var list: std.ArrayList(u32) = .empty;
    defer list.deinit(gpa);

    try list.append(gpa, 1);
    try list.append(gpa, 2);
    try list.append(gpa, 3);

    try std.testing.expectEqual(@as(usize, 3), list.items.len);
    try std.testing.expectEqual(@as(u32, 2), list.items[1]);
}

test "create/destroy pair on the testing allocator" {
    const gpa = std.testing.allocator;
    const p = try gpa.create(u32);
    defer gpa.destroy(p);
    p.* = 7;
    try std.testing.expectEqual(@as(u32, 7), p.*);
}
