const std = @import("std");
const expectEqual = std.testing.expectEqual;

export fn add_c_abi(a: i32, b: i32) i32 {
    return a + b;
}

comptime {
    @export(&sub_c_abi, .{ .name = "sub_c_abi", .linkage = .strong });
}

fn sub_c_abi(a: i32, b: i32) callconv(.c) i32 {
    return a - b;
}

extern "c" fn add_c_abi(a: i32, b: i32) i32;

test "export implies C ABI" {
    try expectEqual(@as(i32, 3), add_c_abi(1, 2));
}

test "typed clobbers syscall wrapper" {
    const rax = asm volatile ("xor %[ret], %[ret]"
        : [ret] "={rax}" (-> usize));
    try expectEqual(@as(usize, 0), rax);
}
