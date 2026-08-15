// intentionally incorrect
// Unbounded comptime recursion exhausts the default branch quota of 1000.
fn loop() void {
    loop();
}

test "comptime infinite recursion" {
    comptime loop();
}
