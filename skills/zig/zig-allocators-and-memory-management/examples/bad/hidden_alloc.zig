// intentionally incorrect
// Hidden allocation: no Allocator parameter means the caller cannot choose
// the policy or free the result. Zig has no implicit heap.
const std = @import("std");

fn duplicate(s: []const u8) []u8 {
    const buf = std.heap.page_allocator.dupe(u8, s) catch unreachable;
    return buf;
}

test "hidden allocation compiles but is a design bug" {
    const dup = duplicate("hello");
    std.heap.page_allocator.free(dup);
}
