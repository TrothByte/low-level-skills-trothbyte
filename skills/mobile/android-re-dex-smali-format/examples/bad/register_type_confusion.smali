# intentionally incorrect smali: register type confusion.
# Local registers p0-p2 are the parameters. The method signature declares
# (ILjava/lang/String;)V — param0=int, param1=String — but this code treats
# the String parameter as an int and moves it with a 32-bit move. The
# register MODEL in smali is untyped (all registers hold 32-bit or 64-bit
# values), but the OPERATIONS must match the ART types.
.class public Lcom/trothbyte/example/WrongType;
.super Ljava/lang/Object;

.method public static checkAge(I)V
    .locals 1

    # p0 = I (int age)
    # WRONG: string compare on an int register — this is a type error the
    # verifier will reject when the class loads.
    const-string v0, "42"
    invoke-virtual {v0, p0}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z
    move-result v0

    # WRONG: using the boolean result as an int constant is nonsense.
    int-to-long v0, v0

    return-void
.end method
