; GOOD example: lit/FileCheck test for the IncrementAddConstantsPass plugin.
;
; Target verification (run on a machine with an LLVM toolchain):
;   llvm-lit path/to/llvm-pass-writing/examples/good
; or manually:
;   clang++ -std=c++17 -O0 -fPIC -fno-rtti -fno-exceptions -shared \
;       %S/increment_add_constants_pass.cpp \
;       $(llvm-config --cxxflags --ldflags --system-libs --libs core support) \
;       -o /tmp/libIncAdd.so
;   opt -passes=verify %s -o /dev/null
;   opt -load-pass-plugin=/tmp/libIncAdd.so -passes=increment-add-constants \
;       -S %s | FileCheck %s
;
; RUN: opt -passes=verify %s -o /dev/null
; RUN: clang++ -std=c++17 -O0 -fPIC -fno-rtti -fno-exceptions -shared \
; RUN:   %S/increment_add_constants_pass.cpp \
; RUN:   $(llvm-config --cxxflags --ldflags --system-libs --libs core support) \
; RUN:   -o %t/libIncAdd.so
; RUN: opt -load-pass-plugin=%t/libIncAdd.so -passes=increment-add-constants \
; RUN:   -S %s | FileCheck %s

define i32 @f(i32 %y) {
entry:
  %a = add i32 5, %y
  ret i32 %a
}

; The pass replaces `add i32 5, %y` with `add i32 6, %y`. The SSA name of the
; new add is auto-generated, so do not anchor the CHECK on %a.
; CHECK-LABEL: define i32 @f(i32 %y)
; CHECK: add i32 6, %y
; CHECK: ret i32
