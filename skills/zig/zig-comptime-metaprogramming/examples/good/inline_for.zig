const std = @import("std");
const expectEqual = std.testing.expectEqual;

fn applyToEach(comptime n: usize, comptime value: u32, x: u32) u32 {
    var result = x;
    inline for (0..n) |_| {
        result += value;
    }
    return result;
}

test "inline for unrolls comptime iteration" {
    try expectEqual(@as(u32, 10), applyToEach(5, 2, 0));
}

fn dispatch(comptime prefix_char: u8, start_value: i32) i32 {
    var result = start_value;
    comptime var i = 0;
    inline while (i < 3) : (i += 1) {
        if ("xyz"[i] == prefix_char) {
            result += @as(i32, i + 1);
        }
    }
    return result;
}

test "inline while with comptime induction variable" {
    try expectEqual(@as(i32, 2), dispatch('x', 1));
    try expectEqual(@as(i32, 1), dispatch('q', 1));
}
