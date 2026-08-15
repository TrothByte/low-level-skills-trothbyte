# intentionally incorrect smali: const-range and register-width confusion.
# A long (64-bit) value needs const-wide; a 32-bit value needs const.
# Using the wrong const form for the width, and spanning more registers
# than allowed, produces bytecode that fails verification.
.class public Lcom/trothbyte/example/BadConst;
.super Ljava/lang/Object;

.method public static makeBits()J
    .locals 2

    # WRONG: a 64-bit long stored with the 32-bit const form. The verifier
    # flags a type conflict (long vs int) in the same register.
    const v0, 0x1000000000

    # WRONG: move-wide needed to copy a 64-bit value; plain move used.
    move v1, v0

    # Return the long register pair (v0, v1) as a J.
    return-wide v0
.end method
