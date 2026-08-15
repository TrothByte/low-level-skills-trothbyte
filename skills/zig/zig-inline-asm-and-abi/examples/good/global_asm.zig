const std = @import("std");
const expectEqual = std.testing.expectEqual;

comptime {
    asm (
        \\.global my_add;
        \\.type my_add, @function;
        \\my_add:
        \\  lea (%rdi,%rsi,1),%eax
        \\  retq
    );
}

extern fn my_add(a: i32, b: i32) i32;

test "global assembly" {
    try expectEqual(@as(i32, 46), my_add(12, 34));
}
