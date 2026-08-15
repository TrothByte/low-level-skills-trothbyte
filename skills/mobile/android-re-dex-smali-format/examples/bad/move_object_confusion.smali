# intentionally incorrect smali: move vs move-object confusion.
# In smali the -object variants (move-object, move-result-object,
# iget-object) move REFERENCE values; the plain variants move 32-bit
# scalars. Swapping them makes the verifier reject the class or, worse,
# silently corrupts the register's type tag in the dex.
.class public Lcom/trothbyte/example/BadMoves;
.super Ljava/lang/Object;

.field private name:Ljava/lang/String;

.method public constructor <init>()V
    .locals 2

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    # WRONG: storing a String reference into an int-scalar register using
    # a plain move. This is a verifier error at load time.
    const-string v0, "Alice"
    move v1, v0

    # WRONG: iget-object (reference) result moved with plain move-result.
    iput-object v1, p0, Lcom/trothbyte/example/BadMoves;->name:Ljava/lang/String;

    return-void
.end method
