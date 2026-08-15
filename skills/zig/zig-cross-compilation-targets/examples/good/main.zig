const std = @import("std");
const builtin = @import("builtin");

pub fn main() !void {
    const arch = builtin.target.cpu.arch;
    const os_tag = builtin.target.os.tag;
    const abi = builtin.target.abi;
    std.debug.print("arch={s} os={s} abi={s}\n", .{
        @tagName(arch),
        @tagName(os_tag),
        @tagName(abi),
    });
}

test "target detection uses builtin.target" {
    const arch = builtin.target.cpu.arch;
    try std.testing.expect(arch == .x86_64 or arch == .aarch64 or arch == .riscv64);
}
