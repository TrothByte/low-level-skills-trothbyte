// intentionally incorrect
// No volatile: the asm has no declared outputs that are used, so the optimizer
// is free to delete the whole block. Side-effecting asm must be volatile.
fn side_effect() void {
    asm ("nop");
}

test "non-volatile asm can be deleted" {
    side_effect();
}
