// intentionally incorrect
// 0.16 made environment variables and process arguments non-global. They are
// reachable only through the std.process.Init parameter of main; this global
// access pattern no longer compiles.
const std = @import("std");

pub fn main() void {
    const allocator = std.heap.page_allocator;
    const argv = std.process.argsAlloc(allocator) catch return;
    defer std.process.argsFree(allocator, argv);
    std.debug.print("argc={d}\n", .{argv.len});
}
