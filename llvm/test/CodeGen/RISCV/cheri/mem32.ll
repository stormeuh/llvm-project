; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: %riscv32_cheri_llc -verify-machineinstrs < %s \
; RUN:   | FileCheck --check-prefix=CHECK-ILP32 %s

declare i32 @llvm.cheri.cap.diff(i8 addrspace(200) *, i8 addrspace(200) *)

; DDC-relative loads

define i32 @ddc_lb(i8 *%ptr) nounwind {
; CHECK-ILP32-LABEL: ddc_lb:
; CHECK-ILP32:       # %bb.0:
; CHECK-ILP32-NEXT:    lb a1, 0(a0)
; CHECK-ILP32-NEXT:    lb a0, 1(a0)
; CHECK-ILP32-NEXT:    mv a0, a1
; CHECK-ILP32-NEXT:    ret
  %1 = load i8, i8 *%ptr
  %2 = sext i8 %1 to i32
  %3 = getelementptr i8, i8 *%ptr, i32 1
  ; Unused to test anyext
  %4 = load volatile i8, i8 *%3
  ret i32 %2
}

define i32 @ddc_lh(i16 *%ptr) nounwind {
; CHECK-ILP32-LABEL: ddc_lh:
; CHECK-ILP32:       # %bb.0:
; CHECK-ILP32-NEXT:    lh a1, 0(a0)
; CHECK-ILP32-NEXT:    lh a0, 4(a0)
; CHECK-ILP32-NEXT:    mv a0, a1
; CHECK-ILP32-NEXT:    ret
  %1 = load i16, i16 *%ptr
  %2 = sext i16 %1 to i32
  %3 = getelementptr i16, i16 *%ptr, i32 2
  ; Unused to test anyext
  %4 = load volatile i16, i16 *%3
  ret i32 %2
}

define i32 @ddc_lw(i32 *%ptr) nounwind {
; CHECK-ILP32-LABEL: ddc_lw:
; CHECK-ILP32:       # %bb.0:
; CHECK-ILP32-NEXT:    lw a1, 0(a0)
; CHECK-ILP32-NEXT:    lw a0, 12(a0)
; CHECK-ILP32-NEXT:    mv a0, a1
; CHECK-ILP32-NEXT:    ret
  %1 = load i32, i32 *%ptr
  %2 = getelementptr i32, i32 *%ptr, i32 3
  %3 = load volatile i32, i32 *%2
  ret i32 %1
}

define i32 @ddc_lbu(i8 *%ptr) nounwind {
; CHECK-ILP32-LABEL: ddc_lbu:
; CHECK-ILP32:       # %bb.0:
; CHECK-ILP32-NEXT:    lbu a1, 0(a0)
; CHECK-ILP32-NEXT:    lbu a0, 4(a0)
; CHECK-ILP32-NEXT:    add a0, a1, a0
; CHECK-ILP32-NEXT:    ret
  %1 = load i8, i8 *%ptr
  %2 = zext i8 %1 to i32
  %3 = getelementptr i8, i8 *%ptr, i32 4
  %4 = load i8, i8 *%3
  %5 = zext i8 %4 to i32
  %6 = add i32 %2, %5
  ret i32 %6
}

define i32 @ddc_lhu(i16 *%ptr) nounwind {
; CHECK-ILP32-LABEL: ddc_lhu:
; CHECK-ILP32:       # %bb.0:
; CHECK-ILP32-NEXT:    lhu a1, 0(a0)
; CHECK-ILP32-NEXT:    lhu a0, 10(a0)
; CHECK-ILP32-NEXT:    add a0, a1, a0
; CHECK-ILP32-NEXT:    ret
  %1 = load i16, i16 *%ptr
  %2 = zext i16 %1 to i32
  %3 = getelementptr i16, i16 *%ptr, i32 5
  %4 = load i16, i16 *%3
  %5 = zext i16 %4 to i32
  %6 = add i32 %2, %5
  ret i32 %6
}

define i32 @ddc_lc(i8 addrspace(200) **%ptr) nounwind {
; CHECK-ILP32-LABEL: ddc_lc:
; CHECK-ILP32:       # %bb.0:
; CHECK-ILP32-NEXT:    lc ca1, 0(a0)
; CHECK-ILP32-NEXT:    lc ca0, 48(a0)
; CHECK-ILP32-NEXT:    csub a0, ca1, ca0
; CHECK-ILP32-NEXT:    ret
  %1 = load i8 addrspace(200) *, i8 addrspace(200) **%ptr
  %2 = getelementptr i8 addrspace(200) *, i8 addrspace(200) **%ptr, i32 6
  %3 = load i8 addrspace(200) *, i8 addrspace(200) **%2
  %4 = call i32 @llvm.cheri.cap.diff(i8 addrspace(200) *%1, i8 addrspace(200) *%3)
  ret i32 %4
}

; DDC-relative stores

define void @ddc_sb(i8 *%ptr, i8 %val) nounwind {
; CHECK-ILP32-LABEL: ddc_sb:
; CHECK-ILP32:       # %bb.0:
; CHECK-ILP32-NEXT:    sb a1, 0(a0)
; CHECK-ILP32-NEXT:    sb a1, 7(a0)
; CHECK-ILP32-NEXT:    ret
  store i8 %val, i8 *%ptr
  %1 = getelementptr i8, i8 *%ptr, i32 7
  store i8 %val, i8 *%1
  ret void
}

define void @ddc_sh(i16 *%ptr, i16 %val) nounwind {
; CHECK-ILP32-LABEL: ddc_sh:
; CHECK-ILP32:       # %bb.0:
; CHECK-ILP32-NEXT:    sh a1, 0(a0)
; CHECK-ILP32-NEXT:    sh a1, 16(a0)
; CHECK-ILP32-NEXT:    ret
  store i16 %val, i16 *%ptr
  %1 = getelementptr i16, i16 *%ptr, i32 8
  store i16 %val, i16 *%1
  ret void
}

define void @ddc_sw(i32 *%ptr, i32 %val) nounwind {
; CHECK-ILP32-LABEL: ddc_sw:
; CHECK-ILP32:       # %bb.0:
; CHECK-ILP32-NEXT:    sw a1, 0(a0)
; CHECK-ILP32-NEXT:    sw a1, 36(a0)
; CHECK-ILP32-NEXT:    ret
  store i32 %val, i32 *%ptr
  %1 = getelementptr i32, i32 *%ptr, i32 9
  store i32 %val, i32 *%1
  ret void
}

define void @ddc_sc(i8 addrspace(200) **%ptr, i8 addrspace(200) *%val) nounwind {
; CHECK-ILP32-LABEL: ddc_sc:
; CHECK-ILP32:       # %bb.0:
; CHECK-ILP32-NEXT:    sc ca1, 0(a0)
; CHECK-ILP32-NEXT:    sc ca1, 80(a0)
; CHECK-ILP32-NEXT:    ret
  store i8 addrspace(200) *%val, i8 addrspace(200) **%ptr
  %1 = getelementptr i8 addrspace(200) *, i8 addrspace(200) **%ptr, i32 10
  store i8 addrspace(200) *%val, i8 addrspace(200) **%1
  ret void
}

; Capability-relative loads

define i32 @cap_lb(i8 addrspace(200) *%cap) nounwind {
; CHECK-ILP32-LABEL: cap_lb:
; CHECK-ILP32:       # %bb.0:
; CHECK-ILP32-NEXT:    lb.cap a1, (ca0)
; CHECK-ILP32-NEXT:    cincoffset ca0, ca0, 11
; CHECK-ILP32-NEXT:    lb.cap a0, (ca0)
; CHECK-ILP32-NEXT:    mv a0, a1
; CHECK-ILP32-NEXT:    ret
  %1 = load i8, i8 addrspace(200) *%cap
  %2 = sext i8 %1 to i32
  %3 = getelementptr i8, i8 addrspace(200) *%cap, i32 11
  ; Unused to test anyext
  %4 = load volatile i8, i8 addrspace(200) *%3
  ret i32 %2
}

define i32 @cap_lh(i16 addrspace(200) *%cap) nounwind {
; CHECK-ILP32-LABEL: cap_lh:
; CHECK-ILP32:       # %bb.0:
; CHECK-ILP32-NEXT:    lh.cap a1, (ca0)
; CHECK-ILP32-NEXT:    cincoffset ca0, ca0, 24
; CHECK-ILP32-NEXT:    lh.cap a0, (ca0)
; CHECK-ILP32-NEXT:    mv a0, a1
; CHECK-ILP32-NEXT:    ret
  %1 = load i16, i16 addrspace(200) *%cap
  %2 = sext i16 %1 to i32
  %3 = getelementptr i16, i16 addrspace(200) *%cap, i32 12
  ; Unused to test anyext
  %4 = load volatile i16, i16 addrspace(200) *%3
  ret i32 %2
}

define i32 @cap_lw(i32 addrspace(200) *%cap) nounwind {
; CHECK-ILP32-LABEL: cap_lw:
; CHECK-ILP32:       # %bb.0:
; CHECK-ILP32-NEXT:    lw.cap a1, (ca0)
; CHECK-ILP32-NEXT:    cincoffset ca0, ca0, 52
; CHECK-ILP32-NEXT:    lw.cap a0, (ca0)
; CHECK-ILP32-NEXT:    mv a0, a1
; CHECK-ILP32-NEXT:    ret
  %1 = load i32, i32 addrspace(200) *%cap
  %2 = getelementptr i32, i32 addrspace(200) *%cap, i32 13
  %3 = load volatile i32, i32 addrspace(200) *%2
  ret i32 %1
}

define i32 @cap_lbu(i8 addrspace(200) *%cap) nounwind {
; CHECK-ILP32-LABEL: cap_lbu:
; CHECK-ILP32:       # %bb.0:
; CHECK-ILP32-NEXT:    lbu.cap a1, (ca0)
; CHECK-ILP32-NEXT:    cincoffset ca0, ca0, 14
; CHECK-ILP32-NEXT:    lbu.cap a0, (ca0)
; CHECK-ILP32-NEXT:    add a0, a1, a0
; CHECK-ILP32-NEXT:    ret
  %1 = load i8, i8 addrspace(200) *%cap
  %2 = zext i8 %1 to i32
  %3 = getelementptr i8, i8 addrspace(200) *%cap, i32 14
  %4 = load i8, i8 addrspace(200) *%3
  %5 = zext i8 %4 to i32
  %6 = add i32 %2, %5
  ret i32 %6
}

define i32 @cap_lhu(i16 addrspace(200) *%cap) nounwind {
; CHECK-ILP32-LABEL: cap_lhu:
; CHECK-ILP32:       # %bb.0:
; CHECK-ILP32-NEXT:    lhu.cap a1, (ca0)
; CHECK-ILP32-NEXT:    cincoffset ca0, ca0, 30
; CHECK-ILP32-NEXT:    lhu.cap a0, (ca0)
; CHECK-ILP32-NEXT:    add a0, a1, a0
; CHECK-ILP32-NEXT:    ret
  %1 = load i16, i16 addrspace(200) *%cap
  %2 = zext i16 %1 to i32
  %3 = getelementptr i16, i16 addrspace(200) *%cap, i32 15
  %4 = load i16, i16 addrspace(200) *%3
  %5 = zext i16 %4 to i32
  %6 = add i32 %2, %5
  ret i32 %6
}

define i32 @cap_lc(i8 addrspace(200) *addrspace(200) *%cap) nounwind {
; CHECK-ILP32-LABEL: cap_lc:
; CHECK-ILP32:       # %bb.0:
; CHECK-ILP32-NEXT:    lc.cap ca1, (ca0)
; CHECK-ILP32-NEXT:    cincoffset ca0, ca0, 128
; CHECK-ILP32-NEXT:    lc.cap ca0, (ca0)
; CHECK-ILP32-NEXT:    csub a0, ca1, ca0
; CHECK-ILP32-NEXT:    ret
  %1 = load i8 addrspace(200) *, i8 addrspace(200) *addrspace(200) *%cap
  %2 = getelementptr i8 addrspace(200) *, i8 addrspace(200) *addrspace(200) *%cap, i32 16
  %3 = load i8 addrspace(200) *, i8 addrspace(200) *addrspace(200) *%2
  %4 = call i32 @llvm.cheri.cap.diff(i8 addrspace(200) *%1, i8 addrspace(200) *%3)
  ret i32 %4
}

; Capability-relative stores

define void @cap_sb(i8 addrspace(200) *%cap, i8 %val) nounwind {
; CHECK-ILP32-LABEL: cap_sb:
; CHECK-ILP32:       # %bb.0:
; CHECK-ILP32-NEXT:    sb.cap a1, (ca0)
; CHECK-ILP32-NEXT:    cincoffset ca0, ca0, 17
; CHECK-ILP32-NEXT:    sb.cap a1, (ca0)
; CHECK-ILP32-NEXT:    ret
  store i8 %val, i8 addrspace(200) *%cap
  %1 = getelementptr i8, i8 addrspace(200) *%cap, i32 17
  store i8 %val, i8 addrspace(200) *%1
  ret void
}

define void @cap_sh(i16 addrspace(200) *%cap, i16 %val) nounwind {
; CHECK-ILP32-LABEL: cap_sh:
; CHECK-ILP32:       # %bb.0:
; CHECK-ILP32-NEXT:    sh.cap a1, (ca0)
; CHECK-ILP32-NEXT:    cincoffset ca0, ca0, 36
; CHECK-ILP32-NEXT:    sh.cap a1, (ca0)
; CHECK-ILP32-NEXT:    ret
  store i16 %val, i16 addrspace(200) *%cap
  %1 = getelementptr i16, i16 addrspace(200) *%cap, i32 18
  store i16 %val, i16 addrspace(200) *%1
  ret void
}

define void @cap_sw(i32 addrspace(200) *%cap, i32 %val) nounwind {
; CHECK-ILP32-LABEL: cap_sw:
; CHECK-ILP32:       # %bb.0:
; CHECK-ILP32-NEXT:    sw.cap a1, (ca0)
; CHECK-ILP32-NEXT:    cincoffset ca0, ca0, 76
; CHECK-ILP32-NEXT:    sw.cap a1, (ca0)
; CHECK-ILP32-NEXT:    ret
  store i32 %val, i32 addrspace(200) *%cap
  %1 = getelementptr i32, i32 addrspace(200) *%cap, i32 19
  store i32 %val, i32 addrspace(200) *%1
  ret void
}

define void @cap_sc(i8 addrspace(200) *addrspace(200) *%cap, i8 addrspace(200) *%val) nounwind {
; CHECK-ILP32-LABEL: cap_sc:
; CHECK-ILP32:       # %bb.0:
; CHECK-ILP32-NEXT:    sc.cap ca1, (ca0)
; CHECK-ILP32-NEXT:    cincoffset ca0, ca0, 160
; CHECK-ILP32-NEXT:    sc.cap ca1, (ca0)
; CHECK-ILP32-NEXT:    ret
  store i8 addrspace(200) *%val, i8 addrspace(200) *addrspace(200) *%cap
  %1 = getelementptr i8 addrspace(200) *, i8 addrspace(200) *addrspace(200) *%cap, i32 20
  store i8 addrspace(200) *%val, i8 addrspace(200) *addrspace(200) *%1
  ret void
}
