const std = @import("std");
const expectEqual = std.testing.expectEqual;

fn countFields(comptime T: type) usize {
    return switch (@typeInfo(T)) {
        .@"struct" => |s| s.fields.len,
        .@"union" => |u| u.fields.len,
        else => @compileError("expected struct or union, found " ++ @typeName(T)),
    };
}

fn max(comptime T: type, a: T, b: T) T {
    return if (a > b) a else b;
}

const Vec2 = struct {
    x: f64,
    y: f64,
};

test "count struct fields via @typeInfo" {
    try expectEqual(@as(usize, 2), countFields(Vec2));
}

test "generic max with comptime type parameter" {
    try expectEqual(@as(i32, 5), max(i32, 5, 3));
    try expectEqual(@as(f64, 2.5), max(f64, 1.5, 2.5));
}

test "field access by comptime string" {
    var v = Vec2{ .x = 1.0, .y = 2.0 };
    @field(v, "x") = 4.0;
    try expectEqual(@as(f64, 4.0), @field(v, "x"));
}
