const std = @import("std");

pub fn main(init: std.process.Init) !void {
    const gpa = init.gpa;
    const io = init.io;

    const ptr = try gpa.create(i32);
    defer gpa.destroy(ptr);
    ptr.* = 42;

    const args = try init.minimal.args.toSlice(init.arena.allocator());
    std.log.info("argc={d}", .{args.len});

    const value = std.fmt.allocPrint(init.arena.allocator(), "gpa={d}", .{ptr.*}) catch "?";
    try std.Io.File.stdout().writeStreamingAll(io, value);
    try std.Io.File.stdout().writeStreamingAll(io, "\n");
}
