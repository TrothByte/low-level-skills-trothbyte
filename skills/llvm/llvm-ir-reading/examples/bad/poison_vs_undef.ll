; BAD example: poison vs undef confusion.
; Regenerate with: clang -O1 -S -emit-llvm poison_vs_undef.c -o poison_vs_undef.ll
; (block names and value names added for readability; structure matches clang output)
;
; The agent misreads `add nsw` overflow as "wraps like unsigned, y becomes INT_MIN,
; so the branch returns 0". Correct reasoning:
;   - `add nsw` that overflows produces POISON, not a wrapped value
;   - icmp on poison is poison, and `br i1 <poison>` is undefined behavior
;   - so for x == INT_MAX this function is simply UB, not "returns 0"

define dso_local i32 @checked(i32 %x) {
entry:
  %add = add nsw i32 %x, 1    ; poison if %x == 2147483647
  %cmp = icmp sgt i32 %add, 0 ; poison propagates through icmp
  br i1 %cmp, label %if.then, label %if.else  ; branching on poison = UB
if.then:
  ret i32 1
if.else:
  ret i32 0
}

; The agent also misreads undef as "a random value that stays the same".
; Every use of an undef value may independently pick any value:
;   %v * %v is undef, NOT a provable square, and NOT provably >= 0.
define dso_local i32 @square_uninit(ptr %p) {
entry:
  %v = load i32, ptr %p, align 4 ; undef if the memory is uninitialized
  %sq = mul i32 %v, %v           ; still undef: the two uses of %v may differ
  ret i32 %sq
}
