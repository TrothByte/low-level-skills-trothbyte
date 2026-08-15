// intentionally incorrect
// Zig 0.16.0 removed @Type (proposal #10710). This 0.15-era reification does
// not compile on 0.16+; use @Struct, @Int, @Union, @Enum, @Pointer, @Fn,
// @Tuple, @EnumLiteral instead.
const std = @import("std");

const Pair = @Type(.{ .@"struct" = .{
    .layout = .auto,
    .fields = &.{
        .{ .name = "x", .type = u32, .default_value_ptr = null, .is_comptime = false, .alignment = @alignOf(u32) },
        .{ .name = "y", .type = u32, .default_value_ptr = null, .is_comptime = false, .alignment = @alignOf(u32) },
    },
    .decls = &.{},
    .is_tuple = false,
} });

test "0.16+ rejects @Type" {
    const p: Pair = .{ .x = 1, .y = 2 };
    try std.testing.expectEqual(@as(u32, 3), p.x + p.y);
}
