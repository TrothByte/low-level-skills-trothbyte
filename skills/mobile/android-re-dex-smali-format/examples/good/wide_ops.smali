# Correct smali: 64-bit (wide) register pairs used consistently.
# v0-v1 hold a long; move-wide copies both registers; const-wide loads a
# 64-bit constant; return-wide matches the declared J type.
.class public Lcom/trothbyte/example/WideOps;
.super Ljava/lang/Object;

.method public static combine(JJ)J
    .locals 2

    # p0-p1 = first long, p2-p3 = second long.
    add-long v0, p0, p2          # v0-v1 = p0-p1 + p2-p3
    move-wide v1, v0             # copy the wide value (v0,v1) <- (v0,v1)

    return-wide v0
.end method

.method public static bigValue()J
    .locals 2

    const-wide v0, 0x1000000000  # 64-bit constant

    return-wide v0
.end method
