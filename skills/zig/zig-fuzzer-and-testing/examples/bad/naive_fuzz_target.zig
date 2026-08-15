// intentionally incorrect
// Naive fuzz target: it validates only the FIRST octet of an IP-like record and
// ignores the rest of the generated input. Every generated input passes, so the
// deep parsing bug (in the trailing bytes) is never reached. The fuzzer cannot
// find failures it is structurally prevented from seeing.
const std = @import("std");

fn parse(record: []const u8) u32 {
    if (record.len < 4) return 0;
    const first = record[0];
    const second = record[1];
    const third = record[2];
    const fourth = record[3];
    if (first > 223) return 0xFFFFFFFF;
    _ = second;
    _ = third;
    _ = fourth;
    return (first << 24) | (second << 16) | (third << 8) | fourth;
}

fn fuzzTest(_: void, smith: *std.testing.Smith) !void {
    var buf: [16]u8 = undefined;
    const len = smith.slice(&buf);
    const first = if (len > 0) buf[0] else 0;
    if (first < 128) {
        try std.testing.expect(parse(buf[0..len]) != 0xFFFFFFFF);
    }
}
