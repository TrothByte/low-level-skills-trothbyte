; GOOD example: read this IR, do not just glance at it.
; Regenerate with: clang -O1 -S -emit-llvm array_index.c -o array_index.ll
; (block names added for readability; structure matches clang output)
;
; Reading checklist:
;   1. Loop: %for.cond holds two phis (%i counter, %s accumulator); the back edge
;      is %for.inc -> %for.cond. Induction value %i starts at 0 and increments by 1.
;   2. sext i32 %i to i64: indices are widened to the native pointer-index width
;      (i64 on x86-64) before GEP. This is normal, not a sign bug.
;   3. GEP: getelementptr inbounds i32, ptr %a, i64 %idx is a + idx*4 bytes.
;      inbounds says the accesses stay within the objects (a and b are arrays).
;   4. mul/add are nsw: signed overflow is assumed impossible (poison otherwise).
;   5. The two loads are independent (different base pointers) -> can be CSE'd or
;      vectorized at higher -O levels.

define dso_local i32 @dot(ptr %a, ptr %b, i32 %n) {
entry:
  br label %for.cond

for.cond:
  %i = phi i32 [ 0, %entry ], [ %inc, %for.inc ]
  %s = phi i32 [ 0, %entry ], [ %add, %for.inc ]
  %cmp = icmp slt i32 %i, %n
  br i1 %cmp, label %for.body, label %for.end

for.body:
  %idxprom = sext i32 %i to i64
  %ap = getelementptr inbounds i32, ptr %a, i64 %idxprom
  %ai = load i32, ptr %ap, align 4
  %bp = getelementptr inbounds i32, ptr %b, i64 %idxprom
  %bi = load i32, ptr %bp, align 4
  %mul = mul nsw i32 %ai, %bi
  %add = add nsw i32 %s, %mul
  br label %for.inc

for.inc:
  %inc = add nsw i32 %i, 1
  br label %for.cond

for.end:
  ret i32 %s
}
