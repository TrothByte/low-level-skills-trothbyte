// intentionally incorrect
// packed struct is bit-packing, not a C struct. The C header defines:
//   struct Pair { uint32_t x; uint8_t tag; };   // size 8, tag at offset 4
// packed { u32, u8 } is 5 bytes, no alignment, wrong for the ABI.
const std = @import("std");

const Pair = packed struct { x: u32, tag: u8 };

test "packed struct is not a C struct" {
    if (@sizeOf(Pair) != 8) @compileError("ABI mismatch with C");
    try std.testing.expectEqual(@as(usize, 5), @sizeOf(Pair));
}
