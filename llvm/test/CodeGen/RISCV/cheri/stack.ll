; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: sed 's/iXLEN/i32/g' %s | %riscv32_cheri_purecap_llc -verify-machineinstrs \
; RUN:   | FileCheck --check-prefix=RV32IXCHERI %s
; RUN: sed 's/iXLEN/i64/g' %s | %riscv64_cheri_purecap_llc -verify-machineinstrs \
; RUN:   | FileCheck --check-prefix=RV64IXCHERI %s

declare i32 @use_arg(i32 addrspace(200)*) addrspace(200)

define i32 @static_alloca() nounwind {
; RV32IXCHERI-LABEL: static_alloca:
; RV32IXCHERI:       # %bb.0:
; RV32IXCHERI-NEXT:    cincoffset csp, csp, -16
; RV32IXCHERI-NEXT:    csc cra, 8(csp)
; RV32IXCHERI-NEXT:    cincoffset ca0, csp, 4
; RV32IXCHERI-NEXT:    csetbounds ca0, ca0, 4
; RV32IXCHERI-NEXT:  .LBB0_1: # Label of block must be emitted
; RV32IXCHERI-NEXT:    auipcc ca1, %captab_pcrel_hi(use_arg)
; RV32IXCHERI-NEXT:    clc ca1, %pcrel_lo(.LBB0_1)(ca1)
; RV32IXCHERI-NEXT:    cjalr ca1
; RV32IXCHERI-NEXT:    clc cra, 8(csp)
; RV32IXCHERI-NEXT:    cincoffset csp, csp, 16
; RV32IXCHERI-NEXT:    cret
;
; RV64IXCHERI-LABEL: static_alloca:
; RV64IXCHERI:       # %bb.0:
; RV64IXCHERI-NEXT:    cincoffset csp, csp, -32
; RV64IXCHERI-NEXT:    csc cra, 16(csp)
; RV64IXCHERI-NEXT:    cincoffset ca0, csp, 12
; RV64IXCHERI-NEXT:    csetbounds ca0, ca0, 4
; RV64IXCHERI-NEXT:  .LBB0_1: # Label of block must be emitted
; RV64IXCHERI-NEXT:    auipcc ca1, %captab_pcrel_hi(use_arg)
; RV64IXCHERI-NEXT:    clc ca1, %pcrel_lo(.LBB0_1)(ca1)
; RV64IXCHERI-NEXT:    cjalr ca1
; RV64IXCHERI-NEXT:    clc cra, 16(csp)
; RV64IXCHERI-NEXT:    cincoffset csp, csp, 32
; RV64IXCHERI-NEXT:    cret
  %var = alloca i32, align 4, addrspace(200)
  %call = call i32 @use_arg(i32 addrspace(200)* nonnull %var)
  ret i32 %call
}

; Check that we are able to handle dynamic allocations.
; Again, because we're at -O0, we get a load of redundant copies.
define i32 @dynamic_alloca(iXLEN %x) nounwind {
; RV32IXCHERI-LABEL: dynamic_alloca:
; RV32IXCHERI:       # %bb.0:
; RV32IXCHERI-NEXT:    cincoffset csp, csp, -16
; RV32IXCHERI-NEXT:    csc cra, 8(csp)
; RV32IXCHERI-NEXT:    csc cs0, 0(csp)
; RV32IXCHERI-NEXT:    cincoffset cs0, csp, 16
; RV32IXCHERI-NEXT:    cgetaddr a1, csp
; RV32IXCHERI-NEXT:    slli a0, a0, 2
; RV32IXCHERI-NEXT:    addi a2, a0, 15
; RV32IXCHERI-NEXT:    andi a2, a2, -16
; RV32IXCHERI-NEXT:    sub a1, a1, a2
; RV32IXCHERI-NEXT:    csetaddr ca1, csp, a1
; RV32IXCHERI-NEXT:    csetbounds ca2, ca1, a2
; RV32IXCHERI-NEXT:    cmove csp, ca1
; RV32IXCHERI-NEXT:    csetbounds ca0, ca2, a0
; RV32IXCHERI-NEXT:  .LBB1_1: # Label of block must be emitted
; RV32IXCHERI-NEXT:    auipcc ca1, %captab_pcrel_hi(use_arg)
; RV32IXCHERI-NEXT:    clc ca1, %pcrel_lo(.LBB1_1)(ca1)
; RV32IXCHERI-NEXT:    cjalr ca1
; RV32IXCHERI-NEXT:    cincoffset csp, cs0, -16
; RV32IXCHERI-NEXT:    clc cs0, 0(csp)
; RV32IXCHERI-NEXT:    clc cra, 8(csp)
; RV32IXCHERI-NEXT:    cincoffset csp, csp, 16
; RV32IXCHERI-NEXT:    cret
;
; RV64IXCHERI-LABEL: dynamic_alloca:
; RV64IXCHERI:       # %bb.0:
; RV64IXCHERI-NEXT:    cincoffset csp, csp, -32
; RV64IXCHERI-NEXT:    csc cra, 16(csp)
; RV64IXCHERI-NEXT:    csc cs0, 0(csp)
; RV64IXCHERI-NEXT:    cincoffset cs0, csp, 32
; RV64IXCHERI-NEXT:    cgetaddr a1, csp
; RV64IXCHERI-NEXT:    slli a0, a0, 2
; RV64IXCHERI-NEXT:    addi a2, a0, 15
; RV64IXCHERI-NEXT:    andi a2, a2, -16
; RV64IXCHERI-NEXT:    crrl a3, a2
; RV64IXCHERI-NEXT:    sub a1, a1, a3
; RV64IXCHERI-NEXT:    cram a2, a2
; RV64IXCHERI-NEXT:    and a1, a1, a2
; RV64IXCHERI-NEXT:    csetaddr ca1, csp, a1
; RV64IXCHERI-NEXT:    csetbounds ca2, ca1, a3
; RV64IXCHERI-NEXT:    cmove csp, ca1
; RV64IXCHERI-NEXT:    csetbounds ca0, ca2, a0
; RV64IXCHERI-NEXT:  .LBB1_1: # Label of block must be emitted
; RV64IXCHERI-NEXT:    auipcc ca1, %captab_pcrel_hi(use_arg)
; RV64IXCHERI-NEXT:    clc ca1, %pcrel_lo(.LBB1_1)(ca1)
; RV64IXCHERI-NEXT:    cjalr ca1
; RV64IXCHERI-NEXT:    cincoffset csp, cs0, -32
; RV64IXCHERI-NEXT:    clc cs0, 0(csp)
; RV64IXCHERI-NEXT:    clc cra, 16(csp)
; RV64IXCHERI-NEXT:    cincoffset csp, csp, 32
; RV64IXCHERI-NEXT:    cret
  %var = alloca i32, iXLEN %x, align 4, addrspace(200)
  %call = call i32 @use_arg(i32 addrspace(200)* nonnull %var)
  ret i32 %call
}
