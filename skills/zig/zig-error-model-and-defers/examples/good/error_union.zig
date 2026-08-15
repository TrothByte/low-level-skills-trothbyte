const std = @import("std");
const expectEqual = std.testing.expectEqual;
const expectError = std.testing.expectError;

const ParseError = error{ EmptyInput, InvalidDigit };

fn parsePositive(s: []const u8) ParseError!u32 {
    if (s.len == 0) return error.EmptyInput;
    var value: u32 = 0;
    for (s) |c| {
        if (c < '0' or c > '9') return error.InvalidDigit;
        value = value * 10 + (c - '0');
    }
    return value;
}

test "try propagates success" {
    try expectEqual(@as(u32, 42), try parsePositive("42"));
}

test "catch handles the error locally" {
    const value = parsePositive("") catch 0;
    try expectEqual(@as(u32, 0), value);
}

test "expectError checks the exact code" {
    try expectError(error.InvalidDigit, parsePositive("4x"));
}

test "errdefer cleanup only on the error path" {
    const gpa = std.testing.allocator;
    const buf = try gpa.alloc(u8, 8);
    errdefer gpa.free(buf);
    try expectEqual(@as(usize, 8), buf.len);
}

test "switch on an error union" {
    const result = parsePositive("12");
    switch (result) {
        error.EmptyInput => try expectEqual(@as(u32, 0), 0),
        error.InvalidDigit => try expectEqual(@as(u32, 0), 0),
        else => |v| try expectEqual(@as(u32, 12), v),
    }
}
