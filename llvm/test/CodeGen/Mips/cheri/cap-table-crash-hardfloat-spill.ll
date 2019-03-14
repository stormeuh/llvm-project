; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: sed 's/addrspace(200)//' %s | %cheri_llc -relocation-model=pic - -o - -O0 | FileCheck -check-prefix N64 %s
; RUN: %cheri_purecap_llc -cheri-cap-table-abi=legacy -o - %s -O0 | FileCheck -check-prefix LEGACY %s
; RUN: %cheri_purecap_llc -cheri-cap-table-abi=pcrel -o - %s -O0 | FileCheck -check-prefix PURECAP %s

; This code now is not quite ideal (we should probably access the constants differently)
; but it mostly matches mips

define float @return_constant_pool() nounwind readnone {
; N64-LABEL: return_constant_pool:
; N64:       # %bb.0: # %entry
; N64-NEXT:    lui $1, %hi(%neg(%gp_rel(return_constant_pool)))
; N64-NEXT:    daddu $1, $1, $25
; N64-NEXT:    daddiu $1, $1, %lo(%neg(%gp_rel(return_constant_pool)))
; N64-NEXT:    ld $1, %got_page(.LCPI0_0)($1)
; N64-NEXT:    lwc1 $f0, %got_ofst(.LCPI0_0)($1)
; N64-NEXT:    jr $ra
; N64-NEXT:    nop

; LEGACY-LABEL: return_constant_pool:
; LEGACY:       # %bb.0: # %entry
; LEGACY-NEXT:    cgetoffset $25, $c12
; LEGACY-NEXT:    lui $1, %hi(%neg(%gp_rel(return_constant_pool)))
; LEGACY-NEXT:    daddu $1, $1, $25
; LEGACY-NEXT:    daddiu $1, $1, %lo(%neg(%gp_rel(return_constant_pool)))
; LEGACY-NEXT:    ld $1, %got_page(.LCPI0_0)($1)
; LEGACY-NEXT:    lwc1 $f0, %got_ofst(.LCPI0_0)($1)
; LEGACY-NEXT:    cjr $c17
; LEGACY-NEXT:    nop
;
; PURECAP-LABEL: return_constant_pool:
; PURECAP:       # %bb.0: # %entry
; PURECAP-NEXT:    lui $1, %hi(%neg(%captab_rel(return_constant_pool)))
; PURECAP-NEXT:    daddiu $1, $1, %lo(%neg(%captab_rel(return_constant_pool)))
; PURECAP-NEXT:    cincoffset $c26, $c12, $1
; PURECAP-NEXT:    cmove $c12, $c26
; PURECAP-NEXT:    clcbi $c12, %captab20(.LCPI0_0)($c12)
; PURECAP-NEXT:    clw $2, $zero, 0($c12)
; PURECAP-NEXT:    mtc1 $2, $f0
; PURECAP-NEXT:    cjr $c17
; PURECAP-NEXT:    nop
entry:
  ret float 8.0
}

define double @return_constant_pool2() nounwind readnone {
; N64-LABEL: return_constant_pool2:
; N64:       # %bb.0: # %entry
; N64-NEXT:    lui $1, %hi(%neg(%gp_rel(return_constant_pool2)))
; N64-NEXT:    daddu $1, $1, $25
; N64-NEXT:    daddiu $1, $1, %lo(%neg(%gp_rel(return_constant_pool2)))
; N64-NEXT:    ld $1, %got_page(.LCPI1_0)($1)
; N64-NEXT:    ldc1 $f0, %got_ofst(.LCPI1_0)($1)
; N64-NEXT:    jr $ra
; N64-NEXT:    nop

; LEGACY-LABEL: return_constant_pool2:
; LEGACY:       # %bb.0: # %entry
; LEGACY-NEXT:    cgetoffset $25, $c12
; LEGACY-NEXT:    lui $1, %hi(%neg(%gp_rel(return_constant_pool2)))
; LEGACY-NEXT:    daddu $1, $1, $25
; LEGACY-NEXT:    daddiu $1, $1, %lo(%neg(%gp_rel(return_constant_pool2)))
; LEGACY-NEXT:    ld $1, %got_page(.LCPI1_0)($1)
; LEGACY-NEXT:    ldc1 $f0, %got_ofst(.LCPI1_0)($1)
; LEGACY-NEXT:    cjr $c17
; LEGACY-NEXT:    nop
;
; PURECAP-LABEL: return_constant_pool2:
; PURECAP:       # %bb.0: # %entry
; PURECAP-NEXT:    lui $1, %hi(%neg(%captab_rel(return_constant_pool2)))
; PURECAP-NEXT:    daddiu $1, $1, %lo(%neg(%captab_rel(return_constant_pool2)))
; PURECAP-NEXT:    cincoffset $c26, $c12, $1
; PURECAP-NEXT:    cmove $c12, $c26
; PURECAP-NEXT:    clcbi $c12, %captab20(.LCPI1_0)($c12)
; PURECAP-NEXT:    cld $1, $zero, 0($c12)
; PURECAP-NEXT:    dmtc1 $1, $f0
; PURECAP-NEXT:    cjr $c17
; PURECAP-NEXT:    nop
entry:
  ret double 12.0
}

; SDValue MipsTargetLowering::lowerConstantPool(SDValue Op, SelectionDAG &DAG) const needs updating
;%struct.a = type { }
;define void @b() {
;  %c = fmul double undef, undef
;  call void @d(%struct.a addrspace(200)* undef, %struct.a addrspace(200)* undef, i32 undef, double %c)
;  unreachable
;}
;declare void @d(%struct.a addrspace(200)*, %struct.a addrspace(200)*, i32 , double)
