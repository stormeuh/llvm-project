; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUNNOT: %cheri_purecap_llc -O2 -start-after=cheri-purecap-alloca %s -o - -debug-only=dagcombine,mips-lower
; RUN: %cheri_purecap_llc -O2 -start-after=cheri-purecap-alloca %s -o - | %cheri_FileCheck %s

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start.p200i8(i64 immarg, i8 addrspace(200)* nocapture) addrspace(200) #1

declare void @use(i8 addrspace(200)*) local_unnamed_addr addrspace(200) #2

; Function Attrs: nounwind readnone
declare i8 addrspace(200)* @llvm.cheri.cap.bounds.set.i64(i8 addrspace(200)*, i64) addrspace(200) #3
declare i8 addrspace(200)* @llvm.cheri.cap.bounds.set.exact.i64(i8 addrspace(200)*, i64) addrspace(200) #3

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end.p200i8(i64 immarg, i8 addrspace(200)* nocapture) addrspace(200) #1

; Function Attrs: nounwind readnone
declare i8 addrspace(200)* @llvm.cheri.bounded.stack.cap.i64(i8 addrspace(200)*, i64) addrspace(200) #3

; Function Attrs: nounwind
define signext i32 @stack_array() local_unnamed_addr addrspace(200) #0 {
; CHECK-LABEL: stack_array:
; CHECK:       # %bb.0: # %entry
; CHECK-NEXT:    cincoffset $c11, $c11, -[[#STACKFRAME_SIZE:]]
; CHECK-NEXT:    csc $c18, $zero, [[#CAP_SIZE * 4]]($c11)
; CHECK-NEXT:    csc $c17, $zero, [[#CAP_SIZE * 3]]($c11)
; CHECK-NEXT:    lui $1, %hi(%neg(%captab_rel(stack_array)))
; CHECK-NEXT:    daddiu $1, $1, %lo(%neg(%captab_rel(stack_array)))
; CHECK-NEXT:    cincoffset $c1, $c12, $1
; CHECK-NEXT:    cincoffset $c18, $c11, 8
; CHECK-NEXT:    csetbounds $c18, $c18, 40
; CHECK-NEXT:    clcbi $c12, %capcall20(use)($c1)
; CHECK-NEXT:    cincoffset $c3, $c11, 8
; CHECK-NEXT:    cjalr $c12, $c17
; CHECK-NEXT:    csetbounds $c3, $c3, 40
; CHECK-NEXT:    clw $2, $zero, 20($c18)
; CHECK-NEXT:    clc $c17, $zero, [[#CAP_SIZE * 3]]($c11)
; CHECK-NEXT:    clc $c18, $zero, [[#CAP_SIZE * 4]]($c11)
; CHECK-NEXT:    cjr $c17
; CHECK-NEXT:    cincoffset $c11, $c11, [[#STACKFRAME_SIZE]]
entry:
  %array = alloca [10 x i32], align 4, addrspace(200)
  %0 = bitcast [10 x i32] addrspace(200)* %array to i8 addrspace(200)*
  %1 = call i8 addrspace(200)* @llvm.cheri.bounded.stack.cap.i64(i8 addrspace(200)* %0, i64 40)
  %2 = bitcast i8 addrspace(200)* %1 to [10 x i32] addrspace(200)*
  %3 = bitcast [10 x i32] addrspace(200)* %2 to i8 addrspace(200)*
  %4 = call i8 addrspace(200)* @llvm.cheri.cap.bounds.set.i64(i8 addrspace(200)* nonnull %3, i64 40)
  call void @use(i8 addrspace(200)* %4) #4
  %arrayidx = getelementptr inbounds i8, i8 addrspace(200)* %4, i64 20
  %5 = bitcast i8 addrspace(200)* %arrayidx to i32 addrspace(200)*
  %6 = load i32, i32 addrspace(200)* %5, align 4
  ret i32 %6
}

; Function Attrs: nounwind
define signext i32 @stack_int() local_unnamed_addr addrspace(200) #0 {
; CHECK-LABEL: stack_int:
; CHECK:       # %bb.0: # %entry
; CHECK-NEXT:    cincoffset $c11, $c11, -[[#STACKFRAME_SIZE:]]
; CHECK-NEXT:    csc $c17, $zero, [[#CAP_SIZE * 1]]($c11)
; CHECK-NEXT:    lui $1, %hi(%neg(%captab_rel(stack_int)))
; CHECK-NEXT:    daddiu $1, $1, %lo(%neg(%captab_rel(stack_int)))
; CHECK-NEXT:    cincoffset $c1, $c12, $1
; CHECK-NEXT:    addiu $1, $zero, 1
; CHECK-NEXT:    csw $1, $zero, 12($c11)
; CHECK-NEXT:    clcbi $c12, %capcall20(use)($c1)
; CHECK-NEXT:    cincoffset $c3, $c11, 12
; CHECK-NEXT:    cjalr $c12, $c17
; CHECK-NEXT:    csetbounds $c3, $c3, 4
; CHECK-NEXT:    clw $2, $zero, 12($c11)
; CHECK-NEXT:    clc $c17, $zero, [[#CAP_SIZE * 1]]($c11)
; CHECK-NEXT:    cjr $c17
; CHECK-NEXT:    cincoffset $c11, $c11, [[#STACKFRAME_SIZE]]
entry:
  %value = alloca i32, align 4, addrspace(200)
  %0 = bitcast i32 addrspace(200)* %value to i8 addrspace(200)*
  %1 = call i8 addrspace(200)* @llvm.cheri.bounded.stack.cap.i64(i8 addrspace(200)* %0, i64 4)
  %2 = bitcast i8 addrspace(200)* %1 to i32 addrspace(200)*
  %3 = bitcast i32 addrspace(200)* %2 to i8 addrspace(200)*
  store i32 1, i32 addrspace(200)* %value, align 4
  %4 = call i8 addrspace(200)* @llvm.cheri.cap.bounds.set.i64(i8 addrspace(200)* nonnull %3, i64 4)
  call void @use(i8 addrspace(200)* %4) #4
  %5 = load i32, i32 addrspace(200)* %value, align 4
  ret i32 %5
}

; Function Attrs: nounwind
define signext i32 @stack_int_exact() local_unnamed_addr addrspace(200) #0 {
; CHECK-LABEL: stack_int_exact:
; CHECK:       # %bb.0: # %entry
; CHECK-NEXT:    cincoffset $c11, $c11, -[[#STACKFRAME_SIZE:]]
; CHECK-NEXT:    csc $c17, $zero, [[#CAP_SIZE * 1]]($c11)
; CHECK-NEXT:    lui $1, %hi(%neg(%captab_rel(stack_int_exact)))
; CHECK-NEXT:    daddiu $1, $1, %lo(%neg(%captab_rel(stack_int_exact)))
; CHECK-NEXT:    cincoffset $c1, $c12, $1
; CHECK-NEXT:    cincoffset $c2, $c11, 12
; CHECK-NEXT:    csetbounds $c2, $c2, 4
; CHECK-NEXT:    addiu $1, $zero, 1
; CHECK-NEXT:    csw $1, $zero, 12($c11)
; CHECK-NEXT:    daddiu $1, $zero, 4
; CHECK-NEXT:    clcbi $c12, %capcall20(use)($c1)
; CHECK-NEXT:    cjalr $c12, $c17
; CHECK-NEXT:    csetboundsexact $c3, $c2, $1
; CHECK-NEXT:    clw $2, $zero, 12($c11)
; CHECK-NEXT:    clc $c17, $zero, [[#CAP_SIZE * 1]]($c11)
; CHECK-NEXT:    cjr $c17
; CHECK-NEXT:    cincoffset $c11, $c11, [[#STACKFRAME_SIZE]]
entry:
  %value = alloca i32, align 4, addrspace(200)
  %0 = bitcast i32 addrspace(200)* %value to i8 addrspace(200)*
  %1 = call i8 addrspace(200)* @llvm.cheri.bounded.stack.cap.i64(i8 addrspace(200)* %0, i64 4)
  %2 = bitcast i8 addrspace(200)* %1 to i32 addrspace(200)*
  %3 = bitcast i32 addrspace(200)* %2 to i8 addrspace(200)*
  store i32 1, i32 addrspace(200)* %value, align 4
  %4 = call i8 addrspace(200)* @llvm.cheri.cap.bounds.set.exact.i64(i8 addrspace(200)* nonnull %3, i64 4)
  call void @use(i8 addrspace(200)* %4) #4
  %5 = load i32, i32 addrspace(200)* %value, align 4
  ret i32 %5
}

attributes #0 = { nounwind }
attributes #1 = { argmemonly nounwind }
attributes #2 = { nounwind }
attributes #3 = { nounwind readnone }
attributes #4 = { nounwind }
