// intentionally incorrect
// Passing a runtime-known value to a comptime parameter is a compile error,
// not a runtime branch.
fn max(comptime T: type, a: T, b: T) T {
    return if (a > b) a else b;
}

test "passing a runtime type to a comptime parameter" {
    var runtime_bool: bool = undefined;
    _ = &runtime_bool;
    _ = max(if (runtime_bool) u32 else i32, 1, 2);
}
