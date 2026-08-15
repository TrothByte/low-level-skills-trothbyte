# intentionally incorrect: Kotlin metadata deobfuscation guess.
# Kotlin metadata lives in the @kotlin.Metadata annotation on classes. The
# data1 array holds the actual Kotlin declarations (functions, properties,
# parameter names). The comment claims the class name inside metadata is
# the "original after R8" — wrong: R8 strips Kotlin metadata (default for
# release builds unless keep rules retain it), so there is no metadata to
# read here, and any "parameter name recovery" from this dex is guesswork.
.class public final Lcom/trothbyte/example/Hidden;
.super Ljava/lang/Object;

# WRONG comment: "kotlin metadata says original name was
#              com.example.UserRepository" — no metadata annotation is
#              present in this class; the claim is fabricated.
.method public static process(Ljava/lang/String;)V
    .locals 0
    return-void
.end method
