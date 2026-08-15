const std = @import("std");
const expectEqual = std.testing.expectEqual;

test "@reduce over a bool vector" {
    const v = @Vector(4, i32){ 1, -1, 1, -1 };
    const positive = v > @as(@Vector(4, i32), @splat(0));
    try expectEqual(@as(bool, false), @reduce(.And, positive));
    try expectEqual(@as(bool, true), @reduce(.Or, positive));
}

test "@shuffle combines two vectors" {
    const a = @Vector(7, u8){ 'o', 'l', 'h', 'e', 'r', 'z', 'w' };
    const b = @Vector(4, u8){ 'w', 'd', '!', 'x' };
    const res: @Vector(6, u8) = @shuffle(u8, a, b, @Vector(6, i32){
        -1, 0, 4, 1, -2, -3,
    });
    try std.testing.expectEqualStrings("world!", &@as([6]u8, res));
}

test "@select chooses per element" {
    const pred = @Vector(4, bool){ true, false, true, false };
    const a = @Vector(4, i32){ 1, 2, 3, 4 };
    const b = @Vector(4, i32){ 10, 20, 30, 40 };
    const chosen = @select(i32, pred, a, b);
    try expectEqual(@as(i32, 1), chosen[0]);
    try expectEqual(@as(i32, 20), chosen[1]);
}
