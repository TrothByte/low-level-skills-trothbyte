; GOOD example: reading optimized IR (attributes, aggregate values, intrinsics).
; Regenerate with: clang -O2 -S -emit-llvm optimized.c -o optimized.ll
; NOTE: the exact attribute set (align/dereferenceable/noundef) is
; version-dependent; always re-run clang to confirm the real output.
;
; Attribute reading:
;   noalias             : memory accessed via %dst/%src is not accessed through
;                         pointers not based on them during the call (C restrict).
;   align 4             : the pointers are at least 4-byte aligned; violating -> poison.
;   dereferenceable(8)  : at least 8 bytes may be speculatively loaded without trapping;
;                         implies nonnull in addrspace 0 and implies noundef.
;   noundef             : the value has no undef/poison bits, otherwise UB.
; Aggregate { i32, i32 }:
;   a first-class aggregate can be loaded/stored/returned as one unit; clang returns
;   small structs in registers on x86-64.

declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg)

define dso_local { i32, i32 } @move(ptr noalias align 4 dereferenceable(8) %dst, ptr noalias align 4 dereferenceable(8) %src) {
entry:
  %0 = load { i32, i32 }, ptr %src, align 4
  store { i32, i32 } %0, ptr %dst, align 4
  ret { i32, i32 } %0
}

define dso_local void @copy_item(ptr %d, ptr %s) {
entry:
  ; 16 bytes = sizeof(struct Item) = { i8, i32, i64 } rounded up to alignment.
  call void @llvm.memcpy.p0.p0.i64(ptr %d, ptr %s, i64 16, i1 false)
  ret void
}
