; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc < %s -mtriple=x86_64-apple-darwin -mattr=+avx512vl | FileCheck %s --check-prefixes=CHECK,AVX512VL
; RUN: llc < %s -mtriple=x86_64-apple-darwin -mattr=+avx512f,+avx512vl,+avx512dq | FileCheck %s --check-prefixes=CHECK,AVX512DQVL

define <4 x i64> @PR32546(<8 x float> %a, <8 x float> %b, <8 x float> %c, <8 x float> %d) {
; AVX512VL-LABEL: PR32546:
; AVX512VL:       ## %bb.0: ## %entry
; AVX512VL-NEXT:    vcmpltps %ymm1, %ymm0, %k0
; AVX512VL-NEXT:    vcmpltps %ymm3, %ymm2, %k1
; AVX512VL-NEXT:    kandw %k0, %k1, %k0
; AVX512VL-NEXT:    kmovw %k0, %eax
; AVX512VL-NEXT:    movzbl %al, %eax
; AVX512VL-NEXT:    vpbroadcastd %eax, %ymm0
; AVX512VL-NEXT:    retq
;
; AVX512DQVL-LABEL: PR32546:
; AVX512DQVL:       ## %bb.0: ## %entry
; AVX512DQVL-NEXT:    vcmpltps %ymm1, %ymm0, %k0
; AVX512DQVL-NEXT:    vcmpltps %ymm3, %ymm2, %k1
; AVX512DQVL-NEXT:    kandb %k0, %k1, %k0
; AVX512DQVL-NEXT:    kmovb %k0, %eax
; AVX512DQVL-NEXT:    vpbroadcastd %eax, %ymm0
; AVX512DQVL-NEXT:    retq
entry:
  %0 = tail call i8 @llvm.x86.avx512.mask.cmp.ps.256(<8 x float> %a, <8 x float> %b, i32 1, i8 -1)
  %1 = tail call i8 @llvm.x86.avx512.mask.cmp.ps.256(<8 x float> %c, <8 x float> %d, i32 1, i8 -1)
  %and17 = and i8 %1, %0
  %and = zext i8 %and17 to i32
  %2 = insertelement <8 x i32> undef, i32 %and, i32 0
  %vecinit7.i = shufflevector <8 x i32> %2, <8 x i32> undef, <8 x i32> zeroinitializer
  %3 = bitcast <8 x i32> %vecinit7.i to <4 x i64>
  ret <4 x i64> %3
}

define void @PR32547(<8 x float> %a, <8 x float> %b, <8 x float> %c, <8 x float> %d, float* %p) {
; CHECK-LABEL: PR32547:
; CHECK:       ## %bb.0: ## %entry
; CHECK-NEXT:    vcmpltps %ymm1, %ymm0, %k0
; CHECK-NEXT:    vcmpltps %ymm3, %ymm2, %k1
; CHECK-NEXT:    kunpckbw %k1, %k0, %k1
; CHECK-NEXT:    vxorps %xmm0, %xmm0, %xmm0
; CHECK-NEXT:    vmovaps %zmm0, (%rdi) {%k1}
; CHECK-NEXT:    vzeroupper
; CHECK-NEXT:    retq
 entry:
   %0 = tail call i8 @llvm.x86.avx512.mask.cmp.ps.256(<8 x float> %a, <8 x float> %b, i32 1, i8 -1)
   %1 = tail call i8 @llvm.x86.avx512.mask.cmp.ps.256(<8 x float> %c, <8 x float> %d, i32 1, i8 -1)
   %conv.i = zext i8 %0 to i16
   %conv.i18 = zext i8 %1 to i16
   %shl = shl nuw i16 %conv.i, 8
   %or = or i16 %shl, %conv.i18
   %2 = bitcast float* %p to <16 x float>*
   %3 = bitcast i16 %or to <16 x i1>
   tail call void @llvm.masked.store.v16f32.p0v16f32(<16 x float> zeroinitializer, <16 x float>* %2, i32 64, <16 x i1> %3)
   ret void
}

define void @PR32547_swap(<8 x float> %a, <8 x float> %b, <8 x float> %c, <8 x float> %d, float* %p) {
; CHECK-LABEL: PR32547_swap:
; CHECK:       ## %bb.0: ## %entry
; CHECK-NEXT:    vcmpltps %ymm1, %ymm0, %k0
; CHECK-NEXT:    vcmpltps %ymm3, %ymm2, %k1
; CHECK-NEXT:    kunpckbw %k1, %k0, %k1
; CHECK-NEXT:    vxorps %xmm0, %xmm0, %xmm0
; CHECK-NEXT:    vmovaps %zmm0, (%rdi) {%k1}
; CHECK-NEXT:    vzeroupper
; CHECK-NEXT:    retq
 entry:
   %0 = tail call i8 @llvm.x86.avx512.mask.cmp.ps.256(<8 x float> %a, <8 x float> %b, i32 1, i8 -1)
   %1 = tail call i8 @llvm.x86.avx512.mask.cmp.ps.256(<8 x float> %c, <8 x float> %d, i32 1, i8 -1)
   %conv.i = zext i8 %0 to i16
   %conv.i18 = zext i8 %1 to i16
   %shl = shl nuw i16 %conv.i, 8
   %or = or i16 %conv.i18, %shl
   %2 = bitcast float* %p to <16 x float>*
   %3 = bitcast i16 %or to <16 x i1>
   tail call void @llvm.masked.store.v16f32.p0v16f32(<16 x float> zeroinitializer, <16 x float>* %2, i32 64, <16 x i1> %3)
   ret void
}

define void @mask_cmp_128(<4 x float> %a, <4 x float> %b, <4 x float> %c, <4 x float> %d, float* %p) {
; AVX512VL-LABEL: mask_cmp_128:
; AVX512VL:       ## %bb.0: ## %entry
; AVX512VL-NEXT:    vcmpltps %xmm1, %xmm0, %k0
; AVX512VL-NEXT:    kmovw %k0, %eax
; AVX512VL-NEXT:    vcmpltps %xmm3, %xmm2, %k0
; AVX512VL-NEXT:    shlb $4, %al
; AVX512VL-NEXT:    kmovw %eax, %k1
; AVX512VL-NEXT:    korw %k1, %k0, %k1
; AVX512VL-NEXT:    vxorps %xmm0, %xmm0, %xmm0
; AVX512VL-NEXT:    vmovaps %ymm0, (%rdi) {%k1}
; AVX512VL-NEXT:    vzeroupper
; AVX512VL-NEXT:    retq
;
; AVX512DQVL-LABEL: mask_cmp_128:
; AVX512DQVL:       ## %bb.0: ## %entry
; AVX512DQVL-NEXT:    vcmpltps %xmm1, %xmm0, %k0
; AVX512DQVL-NEXT:    vcmpltps %xmm3, %xmm2, %k1
; AVX512DQVL-NEXT:    kshiftlb $4, %k0, %k0
; AVX512DQVL-NEXT:    korb %k0, %k1, %k1
; AVX512DQVL-NEXT:    vxorps %xmm0, %xmm0, %xmm0
; AVX512DQVL-NEXT:    vmovaps %ymm0, (%rdi) {%k1}
; AVX512DQVL-NEXT:    vzeroupper
; AVX512DQVL-NEXT:    retq
entry:
  %0 = tail call i8 @llvm.x86.avx512.mask.cmp.ps.128(<4 x float> %a, <4 x float> %b, i32 1, i8 -1)
  %1 = tail call i8 @llvm.x86.avx512.mask.cmp.ps.128(<4 x float> %c, <4 x float> %d, i32 1, i8 -1)
  %shl = shl nuw i8 %0, 4
  %or = or i8 %1, %shl
  %2 = bitcast float* %p to <8 x float>*
  %3 = bitcast i8 %or to <8 x i1>
  tail call void @llvm.masked.store.v8f32.p0v8f32(<8 x float> zeroinitializer, <8 x float>* %2, i32 64, <8 x i1> %3)
  ret void
}

define <16 x float> @mask_cmp_512(<16 x float> %a, <16 x float> %b, <16 x float> %c, <16 x float> %d, float* %p) {
; CHECK-LABEL: mask_cmp_512:
; CHECK:       ## %bb.0: ## %entry
; CHECK-NEXT:    vcmpltps {sae}, %zmm1, %zmm0, %k0
; CHECK-NEXT:    vcmpltps %zmm3, %zmm2, %k1
; CHECK-NEXT:    kxnorw %k1, %k0, %k1
; CHECK-NEXT:    vmovaps (%rdi), %zmm0 {%k1} {z}
; CHECK-NEXT:    retq
 entry:
   %0 = tail call i16 @llvm.x86.avx512.mask.cmp.ps.512(<16 x float> %a, <16 x float> %b, i32 1, i16 -1, i32 8)
   %1 = tail call i16 @llvm.x86.avx512.mask.cmp.ps.512(<16 x float> %c, <16 x float> %d, i32 1, i16 -1, i32 4)
   %2 = bitcast float* %p to <16 x float>*
   %3 = load <16 x float>, <16 x float>* %2
   %4 = xor i16 %0, %1
   %5 = bitcast i16 %4 to <16 x i1>
   %6 = select <16 x i1> %5, <16 x float> zeroinitializer, <16 x float> %3
   ret <16 x float> %6
}
declare i8 @llvm.x86.avx512.mask.cmp.ps.128(<4 x float>, <4 x float>, i32, i8)
declare i8 @llvm.x86.avx512.mask.cmp.ps.256(<8 x float>, <8 x float>, i32, i8)
declare i16 @llvm.x86.avx512.mask.cmp.ps.512(<16 x float>, <16 x float>, i32, i16, i32)
declare void @llvm.masked.store.v8f32.p0v8f32(<8 x float>, <8 x float>*, i32, <8 x i1>)
declare void @llvm.masked.store.v16f32.p0v16f32(<16 x float>, <16 x float>*, i32, <16 x i1>)
