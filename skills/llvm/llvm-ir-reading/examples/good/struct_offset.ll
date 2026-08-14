; GOOD example: struct layout + GEP byte-offset reasoning.
; Regenerate with: clang -O1 -S -emit-llvm struct_offset.c -o struct_offset.ll
; (block names added for readability; structure matches clang output)
;
; Layout of { i8, i32, i64 } on x86-64 SysV:
;   tag   -> byte 0
;   id    -> byte 4  (i32 needs alignment 4; bytes 1..3 are padding)
;   score -> byte 8  (i64 needs alignment 8)
;   size = 16, alignment = 8
;
; The struct index in GEP is a FIELD NUMBER, not a byte offset:
;   i32 2 selects field "score", which lives at byte offset 8.
; To add raw byte offsets, use element type i8:
;   getelementptr i8, ptr %p, i64 8
; These two forms are equivalent here and instcombine may convert between them.

%struct.Item = type { i8, i32, i64 }

define dso_local i64 @get_score(ptr %p) {
entry:
  %score = getelementptr inbounds %struct.Item, ptr %p, i64 0, i32 2
  %0 = load i64, ptr %score, align 8
  ret i64 %0
}
