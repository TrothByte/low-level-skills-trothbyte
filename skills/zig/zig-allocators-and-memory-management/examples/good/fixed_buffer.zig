const std = @import("std");

const MAX_BYTES = 512;

test "fixed buffer allocator never touches the heap" {
    var buf: [MAX_BYTES]u8 = undefined;
    var fba = std.heap.FixedBufferAllocator.init(&buf);
    const allocator = fba.allocator();

    const a = try allocator.alloc(u8, 100);
    defer allocator.free(a);
    @memset(a, 0xAB);

    const b = try allocator.alloc(u8, 100);
    defer allocator.free(b);
    @memset(b, 0xCD);

    try std.testing.expectEqual(@as(u8, 0xAB), a[0]);
    try std.testing.expectEqual(@as(u8, 0xCD), b[0]);
}

test "fixed buffer reports OutOfMemory when exhausted" {
    var buf: [16]u8 = undefined;
    var fba = std.heap.FixedBufferAllocator.init(&buf);
    const allocator = fba.allocator();

    const chunk = try allocator.alloc(u8, 16);
    defer allocator.free(chunk);

    const overflow = allocator.alloc(u8, 4);
    try std.testing.expectError(error.OutOfMemory, overflow);
}
