const std = @import("std");

pub extern "c" fn printf(format: [*:0]const u8, ...) c_int;

export fn zig_add(a: i32, b: i32) i32 {
    return a + b;
}

fn var_sum(count: c_int, ...) callconv(.c) c_int {
    var ap = @cVaStart();
    defer @cVaEnd(&ap);
    var sum: c_int = 0;
    var i: usize = 0;
    while (i < @as(usize, @intCast(count))) : (i += 1) {
        sum += @cVaArg(&ap, c_int);
    }
    return sum;
}

test "variadic C function" {
    _ = printf("hello from zig\n");
    try std.testing.expectEqual(@as(c_int, 6), var_sum(3, @as(c_int, 1), @as(c_int, 2), @as(c_int, 3)));
}

test "exported symbol has C ABI" {
    try std.testing.expectEqual(@as(i32, 3), zig_add(1, 2));
}
