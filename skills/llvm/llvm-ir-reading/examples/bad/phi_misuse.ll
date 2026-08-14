; BAD example: phi misuse.
; Regenerate with: clang -O1 -S -emit-llvm phi_misuse.c -o phi_misuse.ll
; (block names added for readability; structure matches clang output)
;
; The agent misreads
;   %i = phi i32 [ 0, %entry ], [ %inc, %for.inc ]
; as a mutable variable ("i is assigned, then updated in place"). Correct reading:
;   - phi is NOT an assignment; it is a merge that picks ONE incoming value
;     depending on which predecessor edge was just taken
;   - every predecessor of %for.cond must appear exactly once in each phi list
;   - the loop is a cycle of SSA values:
;       entry -> for.cond -> for.body -> for.inc -> for.cond
;   - %s is the accumulator merge; %i is the induction value (0 on entry,
;     %inc on the back edge). These two phis are the whole loop state.

define dso_local i32 @sum(i32 %n) {
entry:
  br label %for.cond

for.cond:
  %i = phi i32 [ 0, %entry ], [ %inc, %for.inc ]
  %s = phi i32 [ 0, %entry ], [ %add, %for.inc ]
  %cmp = icmp slt i32 %i, %n
  br i1 %cmp, label %for.body, label %for.end

for.body:
  %add = add nsw i32 %s, %i
  br label %for.inc

for.inc:
  %inc = add nsw i32 %i, 1
  br label %for.cond

for.end:
  ret i32 %s
}
