; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc -o - -mtriple=aarch64-unknown-freebsd13.0 -relocation-model=static -thread-model=posix %s | FileCheck %s
; Adding support for pointer types in atomicrmw instructions broke The AtomicExpandPass for AArch64
; Check that they are expanded in the same way as integer xchg operations

define dso_local i8* @ptr(i8** %ptr, i8* %val) {
; CHECK-LABEL: ptr:
; CHECK-NEXT:    .cfi_startproc
; CHECK-NEXT:  // %bb.0: // %entry
; CHECK-NEXT:    mov x8, x0
; CHECK-NEXT:  .LBB0_1: // %atomicrmw.start
; CHECK-NEXT:    // =>This Inner Loop Header: Depth=1
; CHECK-NEXT:    ldaxr x0, [x8]
; CHECK-NEXT:    stlxr w9, x1, [x8]
; CHECK-NEXT:    cbnz w9, .LBB0_1
; CHECK-NEXT:  // %bb.2: // %atomicrmw.end
; CHECK-NEXT:    ret
entry:
  %tmp = atomicrmw xchg i8** %ptr, i8* %val seq_cst
  ret i8* %tmp
}

define dso_local i64 @int(i64* %ptr, i64 %val) {
; CHECK-LABEL: int:
; CHECK-NEXT:    .cfi_startproc
; CHECK-NEXT:  // %bb.0: // %entry
; CHECK-NEXT:    mov x8, x0
; CHECK-NEXT:  .LBB1_1: // %atomicrmw.start
; CHECK-NEXT:    // =>This Inner Loop Header: Depth=1
; CHECK-NEXT:    ldaxr x0, [x8]
; CHECK-NEXT:    stlxr w9, x1, [x8]
; CHECK-NEXT:    cbnz w9, .LBB1_1
; CHECK-NEXT:  // %bb.2: // %atomicrmw.end
; CHECK-NEXT:    ret
entry:
  %tmp = atomicrmw xchg i64* %ptr, i64 %val seq_cst
  ret i64 %tmp
}
