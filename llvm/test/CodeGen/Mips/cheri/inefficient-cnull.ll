; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: %cheri_purecap_llc %s -O2 -o - | FileCheck %s
; We used to generate lots of redundant cgetnull instructions here

; Function Attrs: nounwind readnone
declare i8 addrspace(200)* @llvm.cheri.cap.address.set(i8 addrspace(200)*, i64) addrspace(200) #2

; Function Attrs: nounwind readnone
declare i8 addrspace(200)* @llvm.cheri.cap.offset.increment(i8 addrspace(200)*, i64) addrspace(200) #2

; Function Attrs: nounwind readnone
declare i8 addrspace(200)* @llvm.cheri.cap.offset.set(i8 addrspace(200)*, i64) addrspace(200) #2

; Function Attrs: nounwind readnone
declare i64 @llvm.cheri.cap.address.get(i8 addrspace(200)*) addrspace(200) #2

define i8 addrspace(200)* @return_intcap(i8 addrspace(200)* %arg) local_unnamed_addr addrspace(200) nounwind readnone {
; CHECK-LABEL: return_intcap:
; CHECK:       # %bb.0: # %entry
; CHECK-NEXT:    cgetaddr $1, $c3
; CHECK-NEXT:    cjr $c17
; CHECK-NEXT:    cincoffset $c3, $cnull, $1
entry:
  %addr = tail call i64 @llvm.cheri.cap.address.get(i8 addrspace(200)* %arg)
  %result = tail call i8 addrspace(200)* @llvm.cheri.cap.offset.increment(i8 addrspace(200)* null, i64 %addr)
  ret i8 addrspace(200)* %result
}

define i8 addrspace(200)* @set_offset(i8 addrspace(200)* %arg, i64 %offset) local_unnamed_addr addrspace(200) nounwind readnone {
; CHECK-LABEL: set_offset:
; CHECK:       # %bb.0: # %entry
; CHECK-NEXT:    cjr $c17
; CHECK-NEXT:    cincoffset $c3, $cnull, $4
entry:
  %result = tail call i8 addrspace(200)* @llvm.cheri.cap.offset.set(i8 addrspace(200)* null, i64 %offset)
  ret i8 addrspace(200)* %result
}

define i8 addrspace(200)* @set_offset_imm(i8 addrspace(200)* %arg) local_unnamed_addr addrspace(200) nounwind readnone {
; CHECK-LABEL: set_offset_imm:
; CHECK:       # %bb.0: # %entry
; CHECK-NEXT:    cjr $c17
; CHECK-NEXT:    cincoffset $c3, $cnull, 123
entry:
  %result = tail call i8 addrspace(200)* @llvm.cheri.cap.offset.set(i8 addrspace(200)* null, i64 123)
  ret i8 addrspace(200)* %result
}

define i8 addrspace(200)* @inc_offset(i8 addrspace(200)* %arg, i64 %offset) local_unnamed_addr addrspace(200) nounwind readnone {
; CHECK-LABEL: inc_offset:
; CHECK:       # %bb.0: # %entry
; CHECK-NEXT:    cjr $c17
; CHECK-NEXT:    cincoffset $c3, $cnull, $4
entry:
  %result = tail call i8 addrspace(200)* @llvm.cheri.cap.offset.increment(i8 addrspace(200)* null, i64 %offset)
  ret i8 addrspace(200)* %result
}

define i8 addrspace(200)* @inc_offset_imm(i8 addrspace(200)* %arg) local_unnamed_addr addrspace(200) nounwind readnone {
; CHECK-LABEL: inc_offset_imm:
; CHECK:       # %bb.0: # %entry
; CHECK-NEXT:    cjr $c17
; CHECK-NEXT:    cincoffset $c3, $cnull, 123
entry:
  %result = tail call i8 addrspace(200)* @llvm.cheri.cap.offset.increment(i8 addrspace(200)* null, i64 123)
  ret i8 addrspace(200)* %result
}

define i8 addrspace(200)* @set_addr(i8 addrspace(200)* %arg, i64 %offset) local_unnamed_addr addrspace(200) nounwind readnone {
; CHECK-LABEL: set_addr:
; CHECK:       # %bb.0: # %entry
; CHECK-NEXT:    cjr $c17
; CHECK-NEXT:    cincoffset $c3, $cnull, $4
entry:
  %result = tail call i8 addrspace(200)* @llvm.cheri.cap.address.set(i8 addrspace(200)* null, i64 %offset)
  ret i8 addrspace(200)* %result
}

define i8 addrspace(200)* @set_addr_imm(i8 addrspace(200)* %arg) local_unnamed_addr addrspace(200) nounwind readnone {
; CHECK-LABEL: set_addr_imm:
; CHECK:       # %bb.0: # %entry
; CHECK-NEXT:    cjr $c17
; CHECK-NEXT:    cincoffset $c3, $cnull, 123
entry:
  %result = tail call i8 addrspace(200)* @llvm.cheri.cap.address.set(i8 addrspace(200)* null, i64 123)
  ret i8 addrspace(200)* %result
}
