# Correct smali: register usage follows the dex register model and the
# instruction widths match the ART types. References use -object moves,
# longs use const-wide / move-wide, and return instructions match the
# declared method type.
.class public Lcom/trothbyte/example/Correct;
.super Ljava/lang/Object;

.field private name:Ljava/lang/String;
.field private count:I

.method public constructor <init>(Ljava/lang/String;I)V
    .locals 0

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    iput-object p1, p0, Lcom/trothbyte/example/Correct;->name:Ljava/lang/String;
    iput p2, p0, Lcom/trothbyte/example/Correct;->count:I

    return-void
.end method

.method public getGreeting()Ljava/lang/String;
    .locals 1

    iget-object v0, p0, Lcom/trothbyte/example/Correct;->name:Ljava/lang/String;

    # Append the count as a string; result is a reference.
    new-instance v0, Ljava/lang/StringBuilder;
    invoke-direct {v0}, Ljava/lang/StringBuilder;-><init>()V
    iget-object v1, p0, Lcom/trothbyte/example/Correct;->name:Ljava/lang/String;
    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    iget v1, p0, Lcom/trothbyte/example/Correct;->count:I
    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;
    invoke-virtual {v0}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;
    move-result-object v0

    return-object v0
.end method
