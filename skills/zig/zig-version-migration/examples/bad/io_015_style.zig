// intentionally incorrect
// Pre-0.15 std.io idiom. 0.15's Writergate replaced the generic std.io streams
// with std.Io.Reader/Writer; std.io.bufferedWriter and BufferedWriter are gone,
// and File.stdout().writer() now takes a buffer and exposes .interface.
const std = @import("std");

pub fn main() !void {
    const stdout_file = std.fs.File.stdout().writer();
    var bw = std.io.bufferedWriter(stdout_file);
    const stdout = bw.writer();

    try stdout.print("Run `zig build test` to run the tests.\n", .{});

    try bw.flush();
}
