// intentionally incorrect
// Freeing an allocation twice is Illegal Behavior; the testing allocator
// (DebugAllocator-backed) detects the double destroy and the test fails.
const std = @import("std");

test "double free is detected" {
    const gpa = std.testing.allocator;
    const p = try gpa.create(u32);
    gpa.destroy(p);
    gpa.destroy(p);
}
