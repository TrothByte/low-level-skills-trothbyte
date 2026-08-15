# Correct: when Kotlin metadata survives (kept by R8 rules or in debug
# builds), the @Lkotlin/Metadata; annotation carries the original names in
# data1 (d1). Parameter names and local function names come from there,
# NOT from guessing. When the annotation is absent, state that names are
# unrecoverable — do not invent them.
.class public final Lcom/trothbyte/example/UserRepo;
.super Ljava/lang/Object;

.annotation runtime Lkotlin/Metadata;
    value = "name=UserRepo, data1={'fun loadUsers(): List<User>','val cache: Cache'}"
.end annotation

.method public static loadUsers()Ljava/util/List;
    .locals 0
    const/4 v0, 0x0
    return-object v0
.end method
