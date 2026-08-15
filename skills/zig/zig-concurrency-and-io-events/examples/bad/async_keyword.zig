// intentionally incorrect
// async/await keywords (and @frameSize) were removed in Zig 0.15.0. Evented
// concurrency moved into the std.Io interface; this pre-0.15 syntax no longer
// compiles.
const std = @import("std");

async fn fakeAsync(x: u32) u32 {
    return x + 1;
}

test "async keyword on 0.15+" {
    const result = async fakeAsync(41);
    _ = result;
}
