const std = @import("std");
const expectEqual = std.testing.expectEqual;

const Header = extern struct {
    magic: u32,
    version: u16,
    flags: u8,
    reserved: u8,
};

test "extern struct matches C layout" {
    try expectEqual(@as(usize, 4), @offsetOf(Header, "version"));
    try expectEqual(@as(usize, 6), @offsetOf(Header, "flags"));
    try expectEqual(@as(usize, 8), @sizeOf(Header));
}

test "size checks against the C compiler must agree" {
    comptime {
        if (@sizeOf(Header) != 8) @compileError("ABI mismatch");
        if (@offsetOf(Header, "magic") != 0) @compileError("ABI mismatch");
    }
}
