; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: %cheri_purecap_opt -S -instcombine -simplifycfg %s -o - | FileCheck %s -check-prefix IR
; RUN: %cheri_purecap_llc -O2 %s -o - | FileCheck %s -check-prefix ASM

; After fixing valuetracking to look through address set intrinsics,
; SimplifyCFG would create a ptrtoint for this three value check:

; Original C source code:
; typedef __UINTPTR_TYPE__ uintptr_t;
; void finalizer(void (*fn)(void)) {
;   if ((uintptr_t) fn != 0 && (uintptr_t) fn != 1 && (uintptr_t) fn != 2)
;     (fn)();
; }

target datalayout = "E-m:e-pf200:128:128:128:64-i8:8:32-i16:16:32-i64:64-n32:64-S128-A200-P200-G200"
target triple = "cheri-unknown-freebsd"

; Function Attrs: nounwind
define void @cannot_fold_tag_unknown(void () addrspace(200)* %fn) local_unnamed_addr addrspace(200) nounwind {
; ASM-LABEL: cannot_fold_tag_unknown:
; ASM:       # %bb.0: # %entry
; ASM-NEXT:    cbez $c3, .LBB0_4
; ASM-NEXT:    nop
; ASM-NEXT:  # %bb.1: # %land.lhs.true
; ASM-NEXT:    cincoffset $c1, $cnull, 1
; ASM-NEXT:    ceq $1, $c1, $c3
; ASM-NEXT:    bnez $1, .LBB0_4
; ASM-NEXT:    nop
; ASM-NEXT:  # %bb.2: # %land.lhs.true2
; ASM-NEXT:    cincoffset $c1, $cnull, 2
; ASM-NEXT:    ceq $1, $c1, $c3
; ASM-NEXT:    bnez $1, .LBB0_4
; ASM-NEXT:    nop
; ASM-NEXT:  # %bb.3: # %if.then
; ASM-NEXT:    cincoffset $c11, $c11, -[[STACKFRAME_SIZE:16|32]]
; ASM-NEXT:    csc $c17, $zero, 0($c11)
; ASM-NEXT:    cmove $c12, $c3
; ASM-NEXT:    cjalr $c12, $c17
; ASM-NEXT:    nop
; ASM-NEXT:    clc $c17, $zero, 0($c11)
; ASM-NEXT:    cincoffset $c11, $c11, 16
; ASM-NEXT:  .LBB0_4: # %if.end
; ASM-NEXT:    cjr $c17
; ASM-NEXT:    nop
; IR-LABEL: @cannot_fold_tag_unknown(
; IR-NEXT:  entry:
; IR-NEXT:    [[TMP0:%.*]] = bitcast void () addrspace(200)* [[FN:%.*]] to i8 addrspace(200)*
; IR-NEXT:    [[CMP:%.*]] = icmp eq void () addrspace(200)* [[FN]], null
; IR-NEXT:    br i1 [[CMP]], label [[IF_END:%.*]], label [[LAND_LHS_TRUE:%.*]]
; IR:       land.lhs.true:
; IR-NEXT:    [[TMP1:%.*]] = tail call i8 addrspace(200)* @llvm.cheri.cap.address.set.i64(i8 addrspace(200)* null, i64 1)
; IR-NEXT:    [[CMP1:%.*]] = icmp eq i8 addrspace(200)* [[TMP1]], [[TMP0]]
; IR-NEXT:    br i1 [[CMP1]], label [[IF_END]], label [[LAND_LHS_TRUE2:%.*]]
; IR:       land.lhs.true2:
; IR-NEXT:    [[TMP2:%.*]] = tail call i8 addrspace(200)* @llvm.cheri.cap.address.set.i64(i8 addrspace(200)* null, i64 2)
; IR-NEXT:    [[CMP3:%.*]] = icmp eq i8 addrspace(200)* [[TMP2]], [[TMP0]]
; IR-NEXT:    br i1 [[CMP3]], label [[IF_END]], label [[IF_THEN:%.*]]
; IR:       if.then:
; IR-NEXT:    tail call void [[FN]]() #2
; IR-NEXT:    br label [[IF_END]]
; IR:       if.end:
; IR-NEXT:    ret void
entry:
  %0 = bitcast void () addrspace(200)* %fn to i8 addrspace(200)*
  %cmp = icmp eq void () addrspace(200)* %fn, null
  br i1 %cmp, label %if.end, label %land.lhs.true

land.lhs.true:                                    ; preds = %entry
  %1 = tail call i8 addrspace(200)* @llvm.cheri.cap.address.set.i64(i8 addrspace(200)* null, i64 1)
  %cmp1 = icmp eq i8 addrspace(200)* %1, %0
  br i1 %cmp1, label %if.end, label %land.lhs.true2

land.lhs.true2:                                   ; preds = %land.lhs.true
  %2 = tail call i8 addrspace(200)* @llvm.cheri.cap.address.set.i64(i8 addrspace(200)* null, i64 2)
  %cmp3 = icmp eq i8 addrspace(200)* %2, %0
  br i1 %cmp3, label %if.end, label %if.then

if.then:                                          ; preds = %land.lhs.true2
  tail call void %fn() #2
  br label %if.end

if.end:                                           ; preds = %land.lhs.true2, %land.lhs.true, %entry, %if.then
  ret void
}

define void @can_fold_tag_unset(void () addrspace(200)* %fn_tagged) local_unnamed_addr addrspace(200) nounwind {
; IR-LABEL: @can_fold_tag_unset(
; IR-NEXT:  entry:
; IR-NEXT:    [[FN_I8:%.*]] = bitcast void () addrspace(200)* [[FN_TAGGED:%.*]] to i8 addrspace(200)*
; IR-NEXT:    [[TMP0:%.*]] = call i8 addrspace(200)* @llvm.cheri.cap.tag.clear(i8 addrspace(200)* [[FN_I8]])
; IR-NEXT:    [[CMP:%.*]] = icmp eq i8 addrspace(200)* [[TMP0]], null
; IR-NEXT:    br i1 [[CMP]], label [[IF_END:%.*]], label [[LAND_LHS_TRUE:%.*]]
; IR:       land.lhs.true:
; IR-NEXT:    [[TMP1:%.*]] = tail call i8 addrspace(200)* @llvm.cheri.cap.address.set.i64(i8 addrspace(200)* null, i64 1)
; IR-NEXT:    [[CMP1:%.*]] = icmp eq i8 addrspace(200)* [[TMP1]], [[TMP0]]
; IR-NEXT:    br i1 [[CMP1]], label [[IF_END]], label [[LAND_LHS_TRUE2:%.*]]
; IR:       land.lhs.true2:
; IR-NEXT:    [[TMP2:%.*]] = tail call i8 addrspace(200)* @llvm.cheri.cap.address.set.i64(i8 addrspace(200)* null, i64 2)
; IR-NEXT:    [[CMP3:%.*]] = icmp eq i8 addrspace(200)* [[TMP2]], [[TMP0]]
; IR-NEXT:    br i1 [[CMP3]], label [[IF_END]], label [[IF_THEN:%.*]]
; IR:       if.then:
; IR-NEXT:    tail call void [[FN_TAGGED]]() #2
; IR-NEXT:    br label [[IF_END]]
; IR:       if.end:
; IR-NEXT:    ret void
;
; ASM-LABEL: can_fold_tag_unset:
; ASM:       # %bb.0: # %entry
; ASM-NEXT:    ccleartag $c1, $c3
; ASM-NEXT:    cbez $c1, .LBB1_4
; ASM-NEXT:    nop
; ASM-NEXT:  # %bb.1: # %land.lhs.true
; ASM-NEXT:    cincoffset $c2, $cnull, 1
; ASM-NEXT:    ceq $1, $c2, $c1
; ASM-NEXT:    bnez $1, .LBB1_4
; ASM-NEXT:    nop
; ASM-NEXT:  # %bb.2: # %land.lhs.true2
; ASM-NEXT:    cincoffset $c2, $cnull, 2
; ASM-NEXT:    ceq $1, $c2, $c1
; ASM-NEXT:    bnez $1, .LBB1_4
; ASM-NEXT:    nop
; ASM-NEXT:  # %bb.3: # %if.then
; ASM-NEXT:    cincoffset $c11, $c11, -[[STACKFRAME_SIZE:16|32]]
; ASM-NEXT:    csc $c17, $zero, 0($c11)
; ASM-NEXT:    cmove $c12, $c3
; ASM-NEXT:    cjalr $c12, $c17
; ASM-NEXT:    nop
; ASM-NEXT:    clc $c17, $zero, 0($c11)
; ASM-NEXT:    cincoffset $c11, $c11, 16
; ASM-NEXT:  .LBB1_4: # %if.end
; ASM-NEXT:    cjr $c17
; ASM-NEXT:    nop
entry:
  %fn_i8 = bitcast void () addrspace(200)* %fn_tagged to i8 addrspace(200)*
  %0 = call i8 addrspace(200)* @llvm.cheri.cap.tag.clear(i8 addrspace(200)* %fn_i8)
  %cmp = icmp eq i8 addrspace(200)* %0, null
  br i1 %cmp, label %if.end, label %land.lhs.true

land.lhs.true:                                    ; preds = %entry
  %1 = tail call i8 addrspace(200)* @llvm.cheri.cap.address.set.i64(i8 addrspace(200)* null, i64 1)
  %cmp1 = icmp eq i8 addrspace(200)* %1, %0
  br i1 %cmp1, label %if.end, label %land.lhs.true2

land.lhs.true2:                                   ; preds = %land.lhs.true
  %2 = tail call i8 addrspace(200)* @llvm.cheri.cap.address.set.i64(i8 addrspace(200)* null, i64 2)
  %cmp3 = icmp eq i8 addrspace(200)* %2, %0
  br i1 %cmp3, label %if.end, label %if.then

if.then:                                          ; preds = %land.lhs.true2
  tail call void %fn_tagged() #2
  br label %if.end

if.end:                                           ; preds = %land.lhs.true2, %land.lhs.true, %entry, %if.then
  ret void
}

; Function Attrs: nounwind readnone
declare i8 addrspace(200)* @llvm.cheri.cap.address.set.i64(i8 addrspace(200)*, i64) addrspace(200) #1
declare i8 addrspace(200)* @llvm.cheri.cap.tag.clear(i8 addrspace(200)*) addrspace(200) #1

; Function Attrs: nounwind readnone
declare i64 @llvm.cheri.cap.address.get.i64(i8 addrspace(200)*) addrspace(200) #1

; Function Attrs: nounwind readnone
declare i8 addrspace(200)* @llvm.cheri.cap.offset.increment.i64(i8 addrspace(200)*, i64) addrspace(200) #1
