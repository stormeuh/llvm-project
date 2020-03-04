; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc --verify-machineinstrs -mtriple=thumbv8.1m.main-none-eabi -mattr=+mve %s -o - | FileCheck %s -check-prefix=CHECK --check-prefix=CHECK-MVE
; RUN: llc --verify-machineinstrs -mtriple=thumbv8.1m.main-none-eabi %s -o - | FileCheck %s -check-prefix=CHECK --check-prefix=CHECK-NON-MVE

define i64 @shift_left_reg(i64 %x, i64 %y) {
; CHECK-MVE-LABEL: shift_left_reg:
; CHECK-MVE:       @ %bb.0: @ %entry
; CHECK-MVE-NEXT:    lsll r0, r1, r2
; CHECK-MVE-NEXT:    bx lr
;
; CHECK-NON-MVE-LABEL: shift_left_reg:
; CHECK-NON-MVE:       @ %bb.0: @ %entry
; CHECK-NON-MVE-NEXT:    rsb.w r3, r2, #32
; CHECK-NON-MVE-NEXT:    lsls r1, r2
; CHECK-NON-MVE-NEXT:    lsr.w r3, r0, r3
; CHECK-NON-MVE-NEXT:    orrs r1, r3
; CHECK-NON-MVE-NEXT:    subs.w r3, r2, #32
; CHECK-NON-MVE-NEXT:    it pl
; CHECK-NON-MVE-NEXT:    lslpl.w r1, r0, r3
; CHECK-NON-MVE-NEXT:    lsl.w r0, r0, r2
; CHECK-NON-MVE-NEXT:    it pl
; CHECK-NON-MVE-NEXT:    movpl r0, #0
; CHECK-NON-MVE-NEXT:    bx lr
entry:
  %shl = shl i64 %x, %y
  ret i64 %shl
}

define i64 @shift_left_imm(i64 %x) {
; CHECK-MVE-LABEL: shift_left_imm:
; CHECK-MVE:       @ %bb.0: @ %entry
; CHECK-MVE-NEXT:    lsll r0, r1, #3
; CHECK-MVE-NEXT:    bx lr
;
; CHECK-NON-MVE-LABEL: shift_left_imm:
; CHECK-NON-MVE:       @ %bb.0: @ %entry
; CHECK-NON-MVE-NEXT:    lsls r1, r1, #3
; CHECK-NON-MVE-NEXT:    orr.w r1, r1, r0, lsr #29
; CHECK-NON-MVE-NEXT:    lsls r0, r0, #3
; CHECK-NON-MVE-NEXT:    bx lr
entry:
  %shl = shl i64 %x, 3
  ret i64 %shl
}

define i64 @shift_left_imm_big(i64 %x) {
; CHECK-LABEL: shift_left_imm_big:
; CHECK:       @ %bb.0: @ %entry
; CHECK-NEXT:    lsls r1, r0, #16
; CHECK-NEXT:    movs r0, #0
; CHECK-NEXT:    bx lr
entry:
  %shl = shl i64 %x, 48
  ret i64 %shl
}

define i64 @shift_left_imm_big2(i64 %x) {
; CHECK-LABEL: shift_left_imm_big2:
; CHECK:       @ %bb.0: @ %entry
; CHECK-NEXT:    mov r1, r0
; CHECK-NEXT:    movs r0, #0
; CHECK-NEXT:    bx lr
entry:
  %shl = shl i64 %x, 32
  ret i64 %shl
}

define i64 @shift_left_imm_big3(i64 %x) {
; CHECK-LABEL: shift_left_imm_big3:
; CHECK:       @ %bb.0: @ %entry
; CHECK-NEXT:    lsls r1, r0, #1
; CHECK-NEXT:    movs r0, #0
; CHECK-NEXT:    bx lr
entry:
  %shl = shl i64 %x, 33
  ret i64 %shl
}

define i64 @shift_right_reg(i64 %x, i64 %y) {
; CHECK-MVE-LABEL: shift_right_reg:
; CHECK-MVE:       @ %bb.0: @ %entry
; CHECK-MVE-NEXT:    rsbs r2, r2, #0
; CHECK-MVE-NEXT:    lsll r0, r1, r2
; CHECK-MVE-NEXT:    bx lr
;
; CHECK-NON-MVE-LABEL: shift_right_reg:
; CHECK-NON-MVE:       @ %bb.0: @ %entry
; CHECK-NON-MVE-NEXT:    rsb.w r3, r2, #32
; CHECK-NON-MVE-NEXT:    lsrs r0, r2
; CHECK-NON-MVE-NEXT:    lsl.w r3, r1, r3
; CHECK-NON-MVE-NEXT:    orrs r0, r3
; CHECK-NON-MVE-NEXT:    subs.w r3, r2, #32
; CHECK-NON-MVE-NEXT:    it pl
; CHECK-NON-MVE-NEXT:    lsrpl.w r0, r1, r3
; CHECK-NON-MVE-NEXT:    lsr.w r1, r1, r2
; CHECK-NON-MVE-NEXT:    it pl
; CHECK-NON-MVE-NEXT:    movpl r1, #0
; CHECK-NON-MVE-NEXT:    bx lr
entry:
  %shr = lshr i64 %x, %y
  ret i64 %shr
}

define i64 @shift_right_imm(i64 %x) {
; CHECK-MVE-LABEL: shift_right_imm:
; CHECK-MVE:       @ %bb.0: @ %entry
; CHECK-MVE-NEXT:    lsrl r0, r1, #3
; CHECK-MVE-NEXT:    bx lr
;
; CHECK-NON-MVE-LABEL: shift_right_imm:
; CHECK-NON-MVE:       @ %bb.0: @ %entry
; CHECK-NON-MVE-NEXT:    lsrs r0, r0, #3
; CHECK-NON-MVE-NEXT:    orr.w r0, r0, r1, lsl #29
; CHECK-NON-MVE-NEXT:    lsrs r1, r1, #3
; CHECK-NON-MVE-NEXT:    bx lr
entry:
  %shr = lshr i64 %x, 3
  ret i64 %shr
}

define i64 @shift_right_imm_big(i64 %x) {
; CHECK-LABEL: shift_right_imm_big:
; CHECK:       @ %bb.0: @ %entry
; CHECK-NEXT:    lsrs r0, r1, #16
; CHECK-NEXT:    movs r1, #0
; CHECK-NEXT:    bx lr
entry:
  %shr = lshr i64 %x, 48
  ret i64 %shr
}

define i64 @shift_right_imm_big2(i64 %x) {
; CHECK-LABEL: shift_right_imm_big2:
; CHECK:       @ %bb.0: @ %entry
; CHECK-NEXT:    mov r0, r1
; CHECK-NEXT:    movs r1, #0
; CHECK-NEXT:    bx lr
entry:
  %shr = lshr i64 %x, 32
  ret i64 %shr
}

define i64 @shift_right_imm_big3(i64 %x) {
; CHECK-LABEL: shift_right_imm_big3:
; CHECK:       @ %bb.0: @ %entry
; CHECK-NEXT:    lsrs r0, r1, #1
; CHECK-NEXT:    movs r1, #0
; CHECK-NEXT:    bx lr
entry:
  %shr = lshr i64 %x, 33
  ret i64 %shr
}

define i64 @shift_arithmetic_right_reg(i64 %x, i64 %y) {
; CHECK-MVE-LABEL: shift_arithmetic_right_reg:
; CHECK-MVE:       @ %bb.0: @ %entry
; CHECK-MVE-NEXT:    asrl r0, r1, r2
; CHECK-MVE-NEXT:    bx lr
;
; CHECK-NON-MVE-LABEL: shift_arithmetic_right_reg:
; CHECK-NON-MVE:       @ %bb.0: @ %entry
; CHECK-NON-MVE-NEXT:    rsb.w r3, r2, #32
; CHECK-NON-MVE-NEXT:    lsrs r0, r2
; CHECK-NON-MVE-NEXT:    lsl.w r3, r1, r3
; CHECK-NON-MVE-NEXT:    orrs r0, r3
; CHECK-NON-MVE-NEXT:    subs.w r3, r2, #32
; CHECK-NON-MVE-NEXT:    asr.w r2, r1, r2
; CHECK-NON-MVE-NEXT:    it pl
; CHECK-NON-MVE-NEXT:    asrpl.w r0, r1, r3
; CHECK-NON-MVE-NEXT:    it pl
; CHECK-NON-MVE-NEXT:    asrpl r2, r1, #31
; CHECK-NON-MVE-NEXT:    mov r1, r2
; CHECK-NON-MVE-NEXT:    bx lr
entry:
  %shr = ashr i64 %x, %y
  ret i64 %shr
}

define i64 @shift_arithmetic_right_imm(i64 %x) {
; CHECK-MVE-LABEL: shift_arithmetic_right_imm:
; CHECK-MVE:       @ %bb.0: @ %entry
; CHECK-MVE-NEXT:    asrl r0, r1, #3
; CHECK-MVE-NEXT:    bx lr
;
; CHECK-NON-MVE-LABEL: shift_arithmetic_right_imm:
; CHECK-NON-MVE:       @ %bb.0: @ %entry
; CHECK-NON-MVE-NEXT:    lsrs r0, r0, #3
; CHECK-NON-MVE-NEXT:    orr.w r0, r0, r1, lsl #29
; CHECK-NON-MVE-NEXT:    asrs r1, r1, #3
; CHECK-NON-MVE-NEXT:    bx lr
entry:
  %shr = ashr i64 %x, 3
  ret i64 %shr
}

%struct.bar = type { i16, i8, [5 x i8] }

define arm_aapcs_vfpcc void @fn1(%struct.bar* nocapture %a) {
; CHECK-MVE-LABEL: fn1:
; CHECK-MVE:       @ %bb.0: @ %entry
; CHECK-MVE-NEXT:    ldr r2, [r0, #4]
; CHECK-MVE-NEXT:    movs r1, #0
; CHECK-MVE-NEXT:    lsll r2, r1, #8
; CHECK-MVE-NEXT:    strb r1, [r0, #7]
; CHECK-MVE-NEXT:    str.w r2, [r0, #3]
; CHECK-MVE-NEXT:    bx lr
;
; CHECK-NON-MVE-LABEL: fn1:
; CHECK-NON-MVE:       @ %bb.0: @ %entry
; CHECK-NON-MVE-NEXT:    ldr r1, [r0, #4]
; CHECK-NON-MVE-NEXT:    lsrs r2, r1, #24
; CHECK-NON-MVE-NEXT:    lsls r1, r1, #8
; CHECK-NON-MVE-NEXT:    strb r2, [r0, #7]
; CHECK-NON-MVE-NEXT:    str.w r1, [r0, #3]
; CHECK-NON-MVE-NEXT:    bx lr
entry:
  %carey = getelementptr inbounds %struct.bar, %struct.bar* %a, i32 0, i32 2
  %0 = bitcast [5 x i8]* %carey to i40*
  %bf.load = load i40, i40* %0, align 1
  %bf.clear = and i40 %bf.load, -256
  store i40 %bf.clear, i40* %0, align 1
  ret void
}

%struct.a = type { i96 }

define void @lsll_128bit_shift(%struct.a* nocapture %x) local_unnamed_addr #0 {
; CHECK-LABEL: lsll_128bit_shift:
; CHECK:       @ %bb.0: @ %entry
; CHECK-NEXT:    movs r1, #0
; CHECK-NEXT:    strd r1, r1, [r0]
; CHECK-NEXT:    str r1, [r0, #8]
; CHECK-NEXT:    bx lr
entry:
  %0 = bitcast %struct.a* %x to i128*
  %bf.load = load i128, i128* %0, align 8
  %bf.clear4 = and i128 %bf.load, -79228162514264337593543950336
  store i128 %bf.clear4, i128* %0, align 8
  ret void
}

%struct.b = type { i184 }

define void @lsll_256bit_shift(%struct.b* nocapture %x) local_unnamed_addr #0 {
; CHECK-LABEL: lsll_256bit_shift:
; CHECK:       @ %bb.0: @ %entry
; CHECK-NEXT:    movs r1, #0
; CHECK-NEXT:    str r1, [r0, #16]
; CHECK-NEXT:    strd r1, r1, [r0, #8]
; CHECK-NEXT:    strd r1, r1, [r0]
; CHECK-NEXT:    ldrb r1, [r0, #23]
; CHECK-NEXT:    lsls r1, r1, #24
; CHECK-NEXT:    str r1, [r0, #20]
; CHECK-NEXT:    bx lr
entry:
  %0 = bitcast %struct.b* %x to i192*
  %bf.load = load i192, i192* %0, align 8
  %bf.clear4 = and i192 %bf.load, -24519928653854221733733552434404946937899825954937634816
  store i192 %bf.clear4, i192* %0, align 8
  ret void
}
