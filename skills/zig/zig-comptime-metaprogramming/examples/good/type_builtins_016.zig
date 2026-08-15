const std = @import("std");
const expectEqual = std.testing.expectEqual;

test "@Int reifies an integer type" {
    const U10 = @Int(.unsigned, 10);
    try expectEqual(@as(U10, 1023), 1023);
}

test "@Struct reifies a struct type" {
    const Pair = @Struct(.auto, null, &.{ "x", "y" }, &.{ u32, u32 }, &@splat(.{}));
    const p: Pair = .{ .x = 1, .y = 2 };
    try expectEqual(@as(u32, 3), p.x + p.y);
}

test "@EnumLiteral is the type of uncoerced enum literals" {
    const LitType = @EnumLiteral();
    const lit: LitType = .some_tag;
    try expectEqual(true, @TypeOf(.some_tag) == LitType);
    _ = lit;
}

test "@Tuple reifies a tuple type" {
    const T = @Tuple(&.{ u8, [2]u16 });
    const t: T = .{ 1, .{ 2, 3 } };
    try expectEqual(@as(u8, 1), t[0]);
    try expectEqual(@as(u16, 3), t[1][1]);
}
