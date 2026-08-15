const std = @import("std");
const expectEqual = std.testing.expectEqual;

fn fib(index: u32) u32 {
    if (index < 2) return index;
    return fib(index - 1) + fib(index - 2);
}

test "raise the comptime branch quota for bounded recursion" {
    @setEvalBranchQuota(100_000);
    comptime {
        try expectEqual(@as(u32, 6765), fib(20));
    }
}
