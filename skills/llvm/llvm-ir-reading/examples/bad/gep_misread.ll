; BAD example: getelementptr offsets misread.
; Regenerate with: clang -O1 -S -emit-llvm gep_misread.c -o gep_misread.ll
;
; The agent misreads:
;   - getelementptr i32, ptr %a, i64 2        as "+2 bytes"  (WRONG: it is +8 bytes)
;   - getelementptr %struct.Rect, ptr %r, i64 0, i32 1
;                                             as "+1 byte"   (WRONG: field number 1)
; Correct reading:
;   - indices into a pointer/array are multiplied by the element size (4 for i32)
;   - indices into a struct are FIELD NUMBERS; the byte offset comes from layout

%struct.Point = type { i32, i32 }
%struct.Rect = type { %struct.Point, i32, i32 }

define dso_local i32 @get_w(ptr %r) {
entry:
  ; Struct index 1 selects field "w". Layout: struct Point (2 x i32) at bytes 0..7,
  ; so w is at byte offset 8. The first index i64 0 offsets by 0 structs.
  %w = getelementptr inbounds %struct.Rect, ptr %r, i64 0, i32 1
  %0 = load i32, ptr %w, align 4
  ret i32 %0
}

define dso_local i32 @array_second(ptr %a) {
entry:
  ; Array/pointer index 2 on element type i32: byte offset = 2 * 4 = 8.
  %elt = getelementptr inbounds i32, ptr %a, i64 2
  %0 = load i32, ptr %elt, align 4
  ret i32 %0
}
