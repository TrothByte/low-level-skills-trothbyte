const std = @import("std");

pub fn main() !void {
    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const ptr = try allocator.create(i32);
    ptr.* = 42;
    std.debug.print("value={d}\n", .{ptr.*});

    const dup = try allocator.dupe(u8, "arena-backed");
    std.debug.print("dup={s}\n", .{dup});
}
