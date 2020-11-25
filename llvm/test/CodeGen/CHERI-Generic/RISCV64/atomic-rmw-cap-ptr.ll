; NOTE: Assertions have been autogenerated by utils/update_test_checks.py UTC_ARGS: --function-signature --scrub-attributes --force-update
; DO NOT EDIT -- This file was generated from test/CodeGen/CHERI-Generic/Inputs/atomic-rmw-cap-ptr.ll
; Check that we can generate sensible code for atomic operations using capability pointers
; https://github.com/CTSRD-CHERI/llvm-project/issues/470
; RUN: llc -mtriple=riscv64 --relocation-model=pic -target-abi l64pc128d -mattr=+xcheri,+cap-mode,+f,+d -mattr=+a < %s | FileCheck %s --check-prefixes=PURECAP,PURECAP-ATOMICS
; RUN: llc -mtriple=riscv64 --relocation-model=pic -target-abi l64pc128d -mattr=+xcheri,+cap-mode,+f,+d -mattr=-a < %s | FileCheck %s --check-prefixes=PURECAP,PURECAP-LIBCALLS
; RUN_FIXME_THIS_CRASHES: llc -mtriple=riscv64 --relocation-model=pic -target-abi lp64d -mattr=+xcheri,+f,+d -mattr=+a < %s | FileCheck %s --check-prefixes=HYBRID,HYBRID-ATOMICS
; RUN_FIXME_THIS_CRASHES: llc -mtriple=riscv64 --relocation-model=pic -target-abi lp64d -mattr=+xcheri,+f,+d -mattr=-a < %s | FileCheck %s --check-prefixes=HYBRID,HYBRID-LIBCALLS

define i64 @atomic_cap_ptr_xchg(i64 addrspace(200)* %ptr, i64 %val) nounwind {
; PURECAP-ATOMICS-LABEL: atomic_cap_ptr_xchg:
; PURECAP-ATOMICS:       # %bb.0: # %bb
; PURECAP-ATOMICS-NEXT:    camoswap.d.aqrl a0, a1, (ca0)
; PURECAP-ATOMICS-NEXT:    cret
;
; PURECAP-LIBCALLS-LABEL: atomic_cap_ptr_xchg:
; PURECAP-LIBCALLS:       # %bb.0: # %bb
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, -16
; PURECAP-LIBCALLS-NEXT:    csc cra, 0(csp)
; PURECAP-LIBCALLS-NEXT:  .LBB0_1: # %bb
; PURECAP-LIBCALLS-NEXT:    # Label of block must be emitted
; PURECAP-LIBCALLS-NEXT:    auipcc ca3, %captab_pcrel_hi(__atomic_exchange_8)
; PURECAP-LIBCALLS-NEXT:    clc ca3, %pcrel_lo(.LBB0_1)(ca3)
; PURECAP-LIBCALLS-NEXT:    addi a2, zero, 5
; PURECAP-LIBCALLS-NEXT:    cjalr ca3
; PURECAP-LIBCALLS-NEXT:    clc cra, 0(csp)
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, 16
; PURECAP-LIBCALLS-NEXT:    cret
bb:
  %tmp = atomicrmw xchg i64 addrspace(200)* %ptr, i64 %val seq_cst
  ret i64 %tmp
}

define i64 @atomic_cap_ptr_add(i64 addrspace(200)* %ptr, i64 %val) nounwind {
; PURECAP-ATOMICS-LABEL: atomic_cap_ptr_add:
; PURECAP-ATOMICS:       # %bb.0: # %bb
; PURECAP-ATOMICS-NEXT:    camoadd.d.aqrl a0, a1, (ca0)
; PURECAP-ATOMICS-NEXT:    cret
;
; PURECAP-LIBCALLS-LABEL: atomic_cap_ptr_add:
; PURECAP-LIBCALLS:       # %bb.0: # %bb
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, -16
; PURECAP-LIBCALLS-NEXT:    csc cra, 0(csp)
; PURECAP-LIBCALLS-NEXT:  .LBB1_1: # %bb
; PURECAP-LIBCALLS-NEXT:    # Label of block must be emitted
; PURECAP-LIBCALLS-NEXT:    auipcc ca3, %captab_pcrel_hi(__atomic_fetch_add_8)
; PURECAP-LIBCALLS-NEXT:    clc ca3, %pcrel_lo(.LBB1_1)(ca3)
; PURECAP-LIBCALLS-NEXT:    addi a2, zero, 5
; PURECAP-LIBCALLS-NEXT:    cjalr ca3
; PURECAP-LIBCALLS-NEXT:    clc cra, 0(csp)
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, 16
; PURECAP-LIBCALLS-NEXT:    cret
bb:
  %tmp = atomicrmw add i64 addrspace(200)* %ptr, i64 %val seq_cst
  ret i64 %tmp
}

define i64 @atomic_cap_ptr_sub(i64 addrspace(200)* %ptr, i64 %val) nounwind {
; PURECAP-ATOMICS-LABEL: atomic_cap_ptr_sub:
; PURECAP-ATOMICS:       # %bb.0: # %bb
; PURECAP-ATOMICS-NEXT:    neg a1, a1
; PURECAP-ATOMICS-NEXT:    camoadd.d.aqrl a0, a1, (ca0)
; PURECAP-ATOMICS-NEXT:    cret
;
; PURECAP-LIBCALLS-LABEL: atomic_cap_ptr_sub:
; PURECAP-LIBCALLS:       # %bb.0: # %bb
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, -16
; PURECAP-LIBCALLS-NEXT:    csc cra, 0(csp)
; PURECAP-LIBCALLS-NEXT:  .LBB2_1: # %bb
; PURECAP-LIBCALLS-NEXT:    # Label of block must be emitted
; PURECAP-LIBCALLS-NEXT:    auipcc ca3, %captab_pcrel_hi(__atomic_fetch_sub_8)
; PURECAP-LIBCALLS-NEXT:    clc ca3, %pcrel_lo(.LBB2_1)(ca3)
; PURECAP-LIBCALLS-NEXT:    addi a2, zero, 5
; PURECAP-LIBCALLS-NEXT:    cjalr ca3
; PURECAP-LIBCALLS-NEXT:    clc cra, 0(csp)
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, 16
; PURECAP-LIBCALLS-NEXT:    cret
bb:
  %tmp = atomicrmw sub i64 addrspace(200)* %ptr, i64 %val seq_cst
  ret i64 %tmp
}

define i64 @atomic_cap_ptr_and(i64 addrspace(200)* %ptr, i64 %val) nounwind {
; PURECAP-ATOMICS-LABEL: atomic_cap_ptr_and:
; PURECAP-ATOMICS:       # %bb.0: # %bb
; PURECAP-ATOMICS-NEXT:    camoand.d.aqrl a0, a1, (ca0)
; PURECAP-ATOMICS-NEXT:    cret
;
; PURECAP-LIBCALLS-LABEL: atomic_cap_ptr_and:
; PURECAP-LIBCALLS:       # %bb.0: # %bb
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, -16
; PURECAP-LIBCALLS-NEXT:    csc cra, 0(csp)
; PURECAP-LIBCALLS-NEXT:  .LBB3_1: # %bb
; PURECAP-LIBCALLS-NEXT:    # Label of block must be emitted
; PURECAP-LIBCALLS-NEXT:    auipcc ca3, %captab_pcrel_hi(__atomic_fetch_and_8)
; PURECAP-LIBCALLS-NEXT:    clc ca3, %pcrel_lo(.LBB3_1)(ca3)
; PURECAP-LIBCALLS-NEXT:    addi a2, zero, 5
; PURECAP-LIBCALLS-NEXT:    cjalr ca3
; PURECAP-LIBCALLS-NEXT:    clc cra, 0(csp)
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, 16
; PURECAP-LIBCALLS-NEXT:    cret
bb:
  %tmp = atomicrmw and i64 addrspace(200)* %ptr, i64 %val seq_cst
  ret i64 %tmp
}

define i64 @atomic_cap_ptr_nand(i64 addrspace(200)* %ptr, i64 %val) nounwind {
; PURECAP-ATOMICS-LABEL: atomic_cap_ptr_nand:
; PURECAP-ATOMICS:       # %bb.0: # %bb
; PURECAP-ATOMICS-NEXT:  .LBB4_1: # %bb
; PURECAP-ATOMICS-NEXT:    # =>This Inner Loop Header: Depth=1
; PURECAP-ATOMICS-NEXT:    clr.d.aqrl a2, (ca0)
; PURECAP-ATOMICS-NEXT:    and a3, a2, a1
; PURECAP-ATOMICS-NEXT:    not a3, a3
; PURECAP-ATOMICS-NEXT:    csc.d.aqrl a3, a3, (ca0)
; PURECAP-ATOMICS-NEXT:    bnez a3, .LBB4_1
; PURECAP-ATOMICS-NEXT:  # %bb.2: # %bb
; PURECAP-ATOMICS-NEXT:    mv a0, a2
; PURECAP-ATOMICS-NEXT:    cret
;
; PURECAP-LIBCALLS-LABEL: atomic_cap_ptr_nand:
; PURECAP-LIBCALLS:       # %bb.0: # %bb
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, -16
; PURECAP-LIBCALLS-NEXT:    csc cra, 0(csp)
; PURECAP-LIBCALLS-NEXT:  .LBB4_1: # %bb
; PURECAP-LIBCALLS-NEXT:    # Label of block must be emitted
; PURECAP-LIBCALLS-NEXT:    auipcc ca3, %captab_pcrel_hi(__atomic_fetch_nand_8)
; PURECAP-LIBCALLS-NEXT:    clc ca3, %pcrel_lo(.LBB4_1)(ca3)
; PURECAP-LIBCALLS-NEXT:    addi a2, zero, 5
; PURECAP-LIBCALLS-NEXT:    cjalr ca3
; PURECAP-LIBCALLS-NEXT:    clc cra, 0(csp)
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, 16
; PURECAP-LIBCALLS-NEXT:    cret
bb:
  %tmp = atomicrmw nand i64 addrspace(200)* %ptr, i64 %val seq_cst
  ret i64 %tmp
}

define i64 @atomic_cap_ptr_or(i64 addrspace(200)* %ptr, i64 %val) nounwind {
; PURECAP-ATOMICS-LABEL: atomic_cap_ptr_or:
; PURECAP-ATOMICS:       # %bb.0: # %bb
; PURECAP-ATOMICS-NEXT:    camoor.d.aqrl a0, a1, (ca0)
; PURECAP-ATOMICS-NEXT:    cret
;
; PURECAP-LIBCALLS-LABEL: atomic_cap_ptr_or:
; PURECAP-LIBCALLS:       # %bb.0: # %bb
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, -16
; PURECAP-LIBCALLS-NEXT:    csc cra, 0(csp)
; PURECAP-LIBCALLS-NEXT:  .LBB5_1: # %bb
; PURECAP-LIBCALLS-NEXT:    # Label of block must be emitted
; PURECAP-LIBCALLS-NEXT:    auipcc ca3, %captab_pcrel_hi(__atomic_fetch_or_8)
; PURECAP-LIBCALLS-NEXT:    clc ca3, %pcrel_lo(.LBB5_1)(ca3)
; PURECAP-LIBCALLS-NEXT:    addi a2, zero, 5
; PURECAP-LIBCALLS-NEXT:    cjalr ca3
; PURECAP-LIBCALLS-NEXT:    clc cra, 0(csp)
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, 16
; PURECAP-LIBCALLS-NEXT:    cret
bb:
  %tmp = atomicrmw or i64 addrspace(200)* %ptr, i64 %val seq_cst
  ret i64 %tmp
}

define i64 @atomic_cap_ptr_xor(i64 addrspace(200)* %ptr, i64 %val) nounwind {
; PURECAP-ATOMICS-LABEL: atomic_cap_ptr_xor:
; PURECAP-ATOMICS:       # %bb.0: # %bb
; PURECAP-ATOMICS-NEXT:    camoxor.d.aqrl a0, a1, (ca0)
; PURECAP-ATOMICS-NEXT:    cret
;
; PURECAP-LIBCALLS-LABEL: atomic_cap_ptr_xor:
; PURECAP-LIBCALLS:       # %bb.0: # %bb
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, -16
; PURECAP-LIBCALLS-NEXT:    csc cra, 0(csp)
; PURECAP-LIBCALLS-NEXT:  .LBB6_1: # %bb
; PURECAP-LIBCALLS-NEXT:    # Label of block must be emitted
; PURECAP-LIBCALLS-NEXT:    auipcc ca3, %captab_pcrel_hi(__atomic_fetch_xor_8)
; PURECAP-LIBCALLS-NEXT:    clc ca3, %pcrel_lo(.LBB6_1)(ca3)
; PURECAP-LIBCALLS-NEXT:    addi a2, zero, 5
; PURECAP-LIBCALLS-NEXT:    cjalr ca3
; PURECAP-LIBCALLS-NEXT:    clc cra, 0(csp)
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, 16
; PURECAP-LIBCALLS-NEXT:    cret
bb:
  %tmp = atomicrmw xor i64 addrspace(200)* %ptr, i64 %val seq_cst
  ret i64 %tmp
}

define i64 @atomic_cap_ptr_max(i64 addrspace(200)* %ptr, i64 %val) nounwind {
; PURECAP-ATOMICS-LABEL: atomic_cap_ptr_max:
; PURECAP-ATOMICS:       # %bb.0: # %bb
; PURECAP-ATOMICS-NEXT:    camomax.d.aqrl a0, a1, (ca0)
; PURECAP-ATOMICS-NEXT:    cret
;
; PURECAP-LIBCALLS-LABEL: atomic_cap_ptr_max:
; PURECAP-LIBCALLS:       # %bb.0: # %bb
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, -80
; PURECAP-LIBCALLS-NEXT:    csc cra, 64(csp)
; PURECAP-LIBCALLS-NEXT:    csc cs0, 48(csp)
; PURECAP-LIBCALLS-NEXT:    csc cs1, 32(csp)
; PURECAP-LIBCALLS-NEXT:    csc cs2, 16(csp)
; PURECAP-LIBCALLS-NEXT:    mv s0, a1
; PURECAP-LIBCALLS-NEXT:    cmove cs2, ca0
; PURECAP-LIBCALLS-NEXT:    cld a1, 0(ca0)
; PURECAP-LIBCALLS-NEXT:    addi a0, zero, 8
; PURECAP-LIBCALLS-NEXT:    cincoffset ca2, csp, 8
; PURECAP-LIBCALLS-NEXT:    csetbounds cs1, ca2, a0
; PURECAP-LIBCALLS-NEXT:    j .LBB7_2
; PURECAP-LIBCALLS-NEXT:  .LBB7_1: # %atomicrmw.start
; PURECAP-LIBCALLS-NEXT:    # in Loop: Header=BB7_2 Depth=1
; PURECAP-LIBCALLS-NEXT:    csd a1, 8(csp)
; PURECAP-LIBCALLS-NEXT:  .LBB7_5: # %atomicrmw.start
; PURECAP-LIBCALLS-NEXT:    # in Loop: Header=BB7_2 Depth=1
; PURECAP-LIBCALLS-NEXT:    # Label of block must be emitted
; PURECAP-LIBCALLS-NEXT:    auipcc ca5, %captab_pcrel_hi(__atomic_compare_exchange_8)
; PURECAP-LIBCALLS-NEXT:    clc ca5, %pcrel_lo(.LBB7_5)(ca5)
; PURECAP-LIBCALLS-NEXT:    addi a3, zero, 5
; PURECAP-LIBCALLS-NEXT:    addi a4, zero, 5
; PURECAP-LIBCALLS-NEXT:    cmove ca0, cs2
; PURECAP-LIBCALLS-NEXT:    cmove ca1, cs1
; PURECAP-LIBCALLS-NEXT:    cjalr ca5
; PURECAP-LIBCALLS-NEXT:    cld a1, 8(csp)
; PURECAP-LIBCALLS-NEXT:    bnez a0, .LBB7_4
; PURECAP-LIBCALLS-NEXT:  .LBB7_2: # %atomicrmw.start
; PURECAP-LIBCALLS-NEXT:    # =>This Inner Loop Header: Depth=1
; PURECAP-LIBCALLS-NEXT:    mv a2, a1
; PURECAP-LIBCALLS-NEXT:    blt s0, a1, .LBB7_1
; PURECAP-LIBCALLS-NEXT:  # %bb.3: # %atomicrmw.start
; PURECAP-LIBCALLS-NEXT:    # in Loop: Header=BB7_2 Depth=1
; PURECAP-LIBCALLS-NEXT:    mv a2, s0
; PURECAP-LIBCALLS-NEXT:    j .LBB7_1
; PURECAP-LIBCALLS-NEXT:  .LBB7_4: # %atomicrmw.end
; PURECAP-LIBCALLS-NEXT:    mv a0, a1
; PURECAP-LIBCALLS-NEXT:    clc cs2, 16(csp)
; PURECAP-LIBCALLS-NEXT:    clc cs1, 32(csp)
; PURECAP-LIBCALLS-NEXT:    clc cs0, 48(csp)
; PURECAP-LIBCALLS-NEXT:    clc cra, 64(csp)
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, 80
; PURECAP-LIBCALLS-NEXT:    cret
bb:
  %tmp = atomicrmw max i64 addrspace(200)* %ptr, i64 %val seq_cst
  ret i64 %tmp
}

define i64 @atomic_cap_ptr_min(i64 addrspace(200)* %ptr, i64 %val) nounwind {
; PURECAP-ATOMICS-LABEL: atomic_cap_ptr_min:
; PURECAP-ATOMICS:       # %bb.0: # %bb
; PURECAP-ATOMICS-NEXT:    camomin.d.aqrl a0, a1, (ca0)
; PURECAP-ATOMICS-NEXT:    cret
;
; PURECAP-LIBCALLS-LABEL: atomic_cap_ptr_min:
; PURECAP-LIBCALLS:       # %bb.0: # %bb
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, -80
; PURECAP-LIBCALLS-NEXT:    csc cra, 64(csp)
; PURECAP-LIBCALLS-NEXT:    csc cs0, 48(csp)
; PURECAP-LIBCALLS-NEXT:    csc cs1, 32(csp)
; PURECAP-LIBCALLS-NEXT:    csc cs2, 16(csp)
; PURECAP-LIBCALLS-NEXT:    mv s0, a1
; PURECAP-LIBCALLS-NEXT:    cmove cs2, ca0
; PURECAP-LIBCALLS-NEXT:    cld a1, 0(ca0)
; PURECAP-LIBCALLS-NEXT:    addi a0, zero, 8
; PURECAP-LIBCALLS-NEXT:    cincoffset ca2, csp, 8
; PURECAP-LIBCALLS-NEXT:    csetbounds cs1, ca2, a0
; PURECAP-LIBCALLS-NEXT:    j .LBB8_2
; PURECAP-LIBCALLS-NEXT:  .LBB8_1: # %atomicrmw.start
; PURECAP-LIBCALLS-NEXT:    # in Loop: Header=BB8_2 Depth=1
; PURECAP-LIBCALLS-NEXT:    csd a1, 8(csp)
; PURECAP-LIBCALLS-NEXT:  .LBB8_5: # %atomicrmw.start
; PURECAP-LIBCALLS-NEXT:    # in Loop: Header=BB8_2 Depth=1
; PURECAP-LIBCALLS-NEXT:    # Label of block must be emitted
; PURECAP-LIBCALLS-NEXT:    auipcc ca5, %captab_pcrel_hi(__atomic_compare_exchange_8)
; PURECAP-LIBCALLS-NEXT:    clc ca5, %pcrel_lo(.LBB8_5)(ca5)
; PURECAP-LIBCALLS-NEXT:    addi a3, zero, 5
; PURECAP-LIBCALLS-NEXT:    addi a4, zero, 5
; PURECAP-LIBCALLS-NEXT:    cmove ca0, cs2
; PURECAP-LIBCALLS-NEXT:    cmove ca1, cs1
; PURECAP-LIBCALLS-NEXT:    cjalr ca5
; PURECAP-LIBCALLS-NEXT:    cld a1, 8(csp)
; PURECAP-LIBCALLS-NEXT:    bnez a0, .LBB8_4
; PURECAP-LIBCALLS-NEXT:  .LBB8_2: # %atomicrmw.start
; PURECAP-LIBCALLS-NEXT:    # =>This Inner Loop Header: Depth=1
; PURECAP-LIBCALLS-NEXT:    mv a2, a1
; PURECAP-LIBCALLS-NEXT:    bge s0, a1, .LBB8_1
; PURECAP-LIBCALLS-NEXT:  # %bb.3: # %atomicrmw.start
; PURECAP-LIBCALLS-NEXT:    # in Loop: Header=BB8_2 Depth=1
; PURECAP-LIBCALLS-NEXT:    mv a2, s0
; PURECAP-LIBCALLS-NEXT:    j .LBB8_1
; PURECAP-LIBCALLS-NEXT:  .LBB8_4: # %atomicrmw.end
; PURECAP-LIBCALLS-NEXT:    mv a0, a1
; PURECAP-LIBCALLS-NEXT:    clc cs2, 16(csp)
; PURECAP-LIBCALLS-NEXT:    clc cs1, 32(csp)
; PURECAP-LIBCALLS-NEXT:    clc cs0, 48(csp)
; PURECAP-LIBCALLS-NEXT:    clc cra, 64(csp)
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, 80
; PURECAP-LIBCALLS-NEXT:    cret
bb:
  %tmp = atomicrmw min i64 addrspace(200)* %ptr, i64 %val seq_cst
  ret i64 %tmp
}

define i64 @atomic_cap_ptr_umax(i64 addrspace(200)* %ptr, i64 %val) nounwind {
; PURECAP-ATOMICS-LABEL: atomic_cap_ptr_umax:
; PURECAP-ATOMICS:       # %bb.0: # %bb
; PURECAP-ATOMICS-NEXT:    camomaxu.d.aqrl a0, a1, (ca0)
; PURECAP-ATOMICS-NEXT:    cret
;
; PURECAP-LIBCALLS-LABEL: atomic_cap_ptr_umax:
; PURECAP-LIBCALLS:       # %bb.0: # %bb
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, -80
; PURECAP-LIBCALLS-NEXT:    csc cra, 64(csp)
; PURECAP-LIBCALLS-NEXT:    csc cs0, 48(csp)
; PURECAP-LIBCALLS-NEXT:    csc cs1, 32(csp)
; PURECAP-LIBCALLS-NEXT:    csc cs2, 16(csp)
; PURECAP-LIBCALLS-NEXT:    mv s0, a1
; PURECAP-LIBCALLS-NEXT:    cmove cs2, ca0
; PURECAP-LIBCALLS-NEXT:    cld a1, 0(ca0)
; PURECAP-LIBCALLS-NEXT:    addi a0, zero, 8
; PURECAP-LIBCALLS-NEXT:    cincoffset ca2, csp, 8
; PURECAP-LIBCALLS-NEXT:    csetbounds cs1, ca2, a0
; PURECAP-LIBCALLS-NEXT:    j .LBB9_2
; PURECAP-LIBCALLS-NEXT:  .LBB9_1: # %atomicrmw.start
; PURECAP-LIBCALLS-NEXT:    # in Loop: Header=BB9_2 Depth=1
; PURECAP-LIBCALLS-NEXT:    csd a1, 8(csp)
; PURECAP-LIBCALLS-NEXT:  .LBB9_5: # %atomicrmw.start
; PURECAP-LIBCALLS-NEXT:    # in Loop: Header=BB9_2 Depth=1
; PURECAP-LIBCALLS-NEXT:    # Label of block must be emitted
; PURECAP-LIBCALLS-NEXT:    auipcc ca5, %captab_pcrel_hi(__atomic_compare_exchange_8)
; PURECAP-LIBCALLS-NEXT:    clc ca5, %pcrel_lo(.LBB9_5)(ca5)
; PURECAP-LIBCALLS-NEXT:    addi a3, zero, 5
; PURECAP-LIBCALLS-NEXT:    addi a4, zero, 5
; PURECAP-LIBCALLS-NEXT:    cmove ca0, cs2
; PURECAP-LIBCALLS-NEXT:    cmove ca1, cs1
; PURECAP-LIBCALLS-NEXT:    cjalr ca5
; PURECAP-LIBCALLS-NEXT:    cld a1, 8(csp)
; PURECAP-LIBCALLS-NEXT:    bnez a0, .LBB9_4
; PURECAP-LIBCALLS-NEXT:  .LBB9_2: # %atomicrmw.start
; PURECAP-LIBCALLS-NEXT:    # =>This Inner Loop Header: Depth=1
; PURECAP-LIBCALLS-NEXT:    mv a2, a1
; PURECAP-LIBCALLS-NEXT:    bltu s0, a1, .LBB9_1
; PURECAP-LIBCALLS-NEXT:  # %bb.3: # %atomicrmw.start
; PURECAP-LIBCALLS-NEXT:    # in Loop: Header=BB9_2 Depth=1
; PURECAP-LIBCALLS-NEXT:    mv a2, s0
; PURECAP-LIBCALLS-NEXT:    j .LBB9_1
; PURECAP-LIBCALLS-NEXT:  .LBB9_4: # %atomicrmw.end
; PURECAP-LIBCALLS-NEXT:    mv a0, a1
; PURECAP-LIBCALLS-NEXT:    clc cs2, 16(csp)
; PURECAP-LIBCALLS-NEXT:    clc cs1, 32(csp)
; PURECAP-LIBCALLS-NEXT:    clc cs0, 48(csp)
; PURECAP-LIBCALLS-NEXT:    clc cra, 64(csp)
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, 80
; PURECAP-LIBCALLS-NEXT:    cret
bb:
  %tmp = atomicrmw umax i64 addrspace(200)* %ptr, i64 %val seq_cst
  ret i64 %tmp
}

define i64 @atomic_cap_ptr_umin(i64 addrspace(200)* %ptr, i64 %val) nounwind {
; PURECAP-ATOMICS-LABEL: atomic_cap_ptr_umin:
; PURECAP-ATOMICS:       # %bb.0: # %bb
; PURECAP-ATOMICS-NEXT:    camominu.d.aqrl a0, a1, (ca0)
; PURECAP-ATOMICS-NEXT:    cret
;
; PURECAP-LIBCALLS-LABEL: atomic_cap_ptr_umin:
; PURECAP-LIBCALLS:       # %bb.0: # %bb
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, -80
; PURECAP-LIBCALLS-NEXT:    csc cra, 64(csp)
; PURECAP-LIBCALLS-NEXT:    csc cs0, 48(csp)
; PURECAP-LIBCALLS-NEXT:    csc cs1, 32(csp)
; PURECAP-LIBCALLS-NEXT:    csc cs2, 16(csp)
; PURECAP-LIBCALLS-NEXT:    mv s0, a1
; PURECAP-LIBCALLS-NEXT:    cmove cs2, ca0
; PURECAP-LIBCALLS-NEXT:    cld a1, 0(ca0)
; PURECAP-LIBCALLS-NEXT:    addi a0, zero, 8
; PURECAP-LIBCALLS-NEXT:    cincoffset ca2, csp, 8
; PURECAP-LIBCALLS-NEXT:    csetbounds cs1, ca2, a0
; PURECAP-LIBCALLS-NEXT:    j .LBB10_2
; PURECAP-LIBCALLS-NEXT:  .LBB10_1: # %atomicrmw.start
; PURECAP-LIBCALLS-NEXT:    # in Loop: Header=BB10_2 Depth=1
; PURECAP-LIBCALLS-NEXT:    csd a1, 8(csp)
; PURECAP-LIBCALLS-NEXT:  .LBB10_5: # %atomicrmw.start
; PURECAP-LIBCALLS-NEXT:    # in Loop: Header=BB10_2 Depth=1
; PURECAP-LIBCALLS-NEXT:    # Label of block must be emitted
; PURECAP-LIBCALLS-NEXT:    auipcc ca5, %captab_pcrel_hi(__atomic_compare_exchange_8)
; PURECAP-LIBCALLS-NEXT:    clc ca5, %pcrel_lo(.LBB10_5)(ca5)
; PURECAP-LIBCALLS-NEXT:    addi a3, zero, 5
; PURECAP-LIBCALLS-NEXT:    addi a4, zero, 5
; PURECAP-LIBCALLS-NEXT:    cmove ca0, cs2
; PURECAP-LIBCALLS-NEXT:    cmove ca1, cs1
; PURECAP-LIBCALLS-NEXT:    cjalr ca5
; PURECAP-LIBCALLS-NEXT:    cld a1, 8(csp)
; PURECAP-LIBCALLS-NEXT:    bnez a0, .LBB10_4
; PURECAP-LIBCALLS-NEXT:  .LBB10_2: # %atomicrmw.start
; PURECAP-LIBCALLS-NEXT:    # =>This Inner Loop Header: Depth=1
; PURECAP-LIBCALLS-NEXT:    mv a2, a1
; PURECAP-LIBCALLS-NEXT:    bgeu s0, a1, .LBB10_1
; PURECAP-LIBCALLS-NEXT:  # %bb.3: # %atomicrmw.start
; PURECAP-LIBCALLS-NEXT:    # in Loop: Header=BB10_2 Depth=1
; PURECAP-LIBCALLS-NEXT:    mv a2, s0
; PURECAP-LIBCALLS-NEXT:    j .LBB10_1
; PURECAP-LIBCALLS-NEXT:  .LBB10_4: # %atomicrmw.end
; PURECAP-LIBCALLS-NEXT:    mv a0, a1
; PURECAP-LIBCALLS-NEXT:    clc cs2, 16(csp)
; PURECAP-LIBCALLS-NEXT:    clc cs1, 32(csp)
; PURECAP-LIBCALLS-NEXT:    clc cs0, 48(csp)
; PURECAP-LIBCALLS-NEXT:    clc cra, 64(csp)
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, 80
; PURECAP-LIBCALLS-NEXT:    cret
bb:
  %tmp = atomicrmw umin i64 addrspace(200)* %ptr, i64 %val seq_cst
  ret i64 %tmp
}

define float @atomic_cap_ptr_fadd(float addrspace(200)* %ptr, float %val) nounwind {
; PURECAP-ATOMICS-LABEL: atomic_cap_ptr_fadd:
; PURECAP-ATOMICS:       # %bb.0: # %bb
; PURECAP-ATOMICS-NEXT:    cflw ft0, 0(ca0)
; PURECAP-ATOMICS-NEXT:  .LBB11_1: # %atomicrmw.start
; PURECAP-ATOMICS-NEXT:    # =>This Loop Header: Depth=1
; PURECAP-ATOMICS-NEXT:    # Child Loop BB11_3 Depth 2
; PURECAP-ATOMICS-NEXT:    fadd.s ft1, ft0, fa0
; PURECAP-ATOMICS-NEXT:    fmv.x.w a1, ft1
; PURECAP-ATOMICS-NEXT:    fmv.x.w a2, ft0
; PURECAP-ATOMICS-NEXT:  .LBB11_3: # %atomicrmw.start
; PURECAP-ATOMICS-NEXT:    # Parent Loop BB11_1 Depth=1
; PURECAP-ATOMICS-NEXT:    # => This Inner Loop Header: Depth=2
; PURECAP-ATOMICS-NEXT:    clr.w.aqrl a3, (ca0)
; PURECAP-ATOMICS-NEXT:    bne a3, a2, .LBB11_5
; PURECAP-ATOMICS-NEXT:  # %bb.4: # %atomicrmw.start
; PURECAP-ATOMICS-NEXT:    # in Loop: Header=BB11_3 Depth=2
; PURECAP-ATOMICS-NEXT:    csc.w.aqrl a4, a1, (ca0)
; PURECAP-ATOMICS-NEXT:    bnez a4, .LBB11_3
; PURECAP-ATOMICS-NEXT:  .LBB11_5: # %atomicrmw.start
; PURECAP-ATOMICS-NEXT:    # in Loop: Header=BB11_1 Depth=1
; PURECAP-ATOMICS-NEXT:    fmv.w.x ft0, a3
; PURECAP-ATOMICS-NEXT:    bne a3, a2, .LBB11_1
; PURECAP-ATOMICS-NEXT:  # %bb.2: # %atomicrmw.end
; PURECAP-ATOMICS-NEXT:    fmv.s fa0, ft0
; PURECAP-ATOMICS-NEXT:    cret
;
; PURECAP-LIBCALLS-LABEL: atomic_cap_ptr_fadd:
; PURECAP-LIBCALLS:       # %bb.0: # %bb
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, -64
; PURECAP-LIBCALLS-NEXT:    csc cra, 48(csp)
; PURECAP-LIBCALLS-NEXT:    csc cs0, 32(csp)
; PURECAP-LIBCALLS-NEXT:    csc cs1, 16(csp)
; PURECAP-LIBCALLS-NEXT:    cfsd fs0, 8(csp)
; PURECAP-LIBCALLS-NEXT:    fmv.s fs0, fa0
; PURECAP-LIBCALLS-NEXT:    cmove cs0, ca0
; PURECAP-LIBCALLS-NEXT:    cflw fa0, 0(ca0)
; PURECAP-LIBCALLS-NEXT:    addi a0, zero, 4
; PURECAP-LIBCALLS-NEXT:    cincoffset ca1, csp, 4
; PURECAP-LIBCALLS-NEXT:    csetbounds cs1, ca1, a0
; PURECAP-LIBCALLS-NEXT:  .LBB11_1: # %atomicrmw.start
; PURECAP-LIBCALLS-NEXT:    # =>This Inner Loop Header: Depth=1
; PURECAP-LIBCALLS-NEXT:    fadd.s ft0, fa0, fs0
; PURECAP-LIBCALLS-NEXT:    cfsw fa0, 4(csp)
; PURECAP-LIBCALLS-NEXT:    fmv.x.w a2, ft0
; PURECAP-LIBCALLS-NEXT:  .LBB11_3: # %atomicrmw.start
; PURECAP-LIBCALLS-NEXT:    # in Loop: Header=BB11_1 Depth=1
; PURECAP-LIBCALLS-NEXT:    # Label of block must be emitted
; PURECAP-LIBCALLS-NEXT:    auipcc ca5, %captab_pcrel_hi(__atomic_compare_exchange_4)
; PURECAP-LIBCALLS-NEXT:    clc ca5, %pcrel_lo(.LBB11_3)(ca5)
; PURECAP-LIBCALLS-NEXT:    addi a3, zero, 5
; PURECAP-LIBCALLS-NEXT:    addi a4, zero, 5
; PURECAP-LIBCALLS-NEXT:    cmove ca0, cs0
; PURECAP-LIBCALLS-NEXT:    cmove ca1, cs1
; PURECAP-LIBCALLS-NEXT:    cjalr ca5
; PURECAP-LIBCALLS-NEXT:    cflw fa0, 4(csp)
; PURECAP-LIBCALLS-NEXT:    beqz a0, .LBB11_1
; PURECAP-LIBCALLS-NEXT:  # %bb.2: # %atomicrmw.end
; PURECAP-LIBCALLS-NEXT:    cfld fs0, 8(csp)
; PURECAP-LIBCALLS-NEXT:    clc cs1, 16(csp)
; PURECAP-LIBCALLS-NEXT:    clc cs0, 32(csp)
; PURECAP-LIBCALLS-NEXT:    clc cra, 48(csp)
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, 64
; PURECAP-LIBCALLS-NEXT:    cret
bb:
  %tmp = atomicrmw fadd float addrspace(200)* %ptr, float %val seq_cst
  ret float %tmp
}

define float @atomic_cap_ptr_fsub(float addrspace(200)* %ptr, float %val) nounwind {
; PURECAP-ATOMICS-LABEL: atomic_cap_ptr_fsub:
; PURECAP-ATOMICS:       # %bb.0: # %bb
; PURECAP-ATOMICS-NEXT:    cflw ft0, 0(ca0)
; PURECAP-ATOMICS-NEXT:  .LBB12_1: # %atomicrmw.start
; PURECAP-ATOMICS-NEXT:    # =>This Loop Header: Depth=1
; PURECAP-ATOMICS-NEXT:    # Child Loop BB12_3 Depth 2
; PURECAP-ATOMICS-NEXT:    fsub.s ft1, ft0, fa0
; PURECAP-ATOMICS-NEXT:    fmv.x.w a1, ft1
; PURECAP-ATOMICS-NEXT:    fmv.x.w a2, ft0
; PURECAP-ATOMICS-NEXT:  .LBB12_3: # %atomicrmw.start
; PURECAP-ATOMICS-NEXT:    # Parent Loop BB12_1 Depth=1
; PURECAP-ATOMICS-NEXT:    # => This Inner Loop Header: Depth=2
; PURECAP-ATOMICS-NEXT:    clr.w.aqrl a3, (ca0)
; PURECAP-ATOMICS-NEXT:    bne a3, a2, .LBB12_5
; PURECAP-ATOMICS-NEXT:  # %bb.4: # %atomicrmw.start
; PURECAP-ATOMICS-NEXT:    # in Loop: Header=BB12_3 Depth=2
; PURECAP-ATOMICS-NEXT:    csc.w.aqrl a4, a1, (ca0)
; PURECAP-ATOMICS-NEXT:    bnez a4, .LBB12_3
; PURECAP-ATOMICS-NEXT:  .LBB12_5: # %atomicrmw.start
; PURECAP-ATOMICS-NEXT:    # in Loop: Header=BB12_1 Depth=1
; PURECAP-ATOMICS-NEXT:    fmv.w.x ft0, a3
; PURECAP-ATOMICS-NEXT:    bne a3, a2, .LBB12_1
; PURECAP-ATOMICS-NEXT:  # %bb.2: # %atomicrmw.end
; PURECAP-ATOMICS-NEXT:    fmv.s fa0, ft0
; PURECAP-ATOMICS-NEXT:    cret
;
; PURECAP-LIBCALLS-LABEL: atomic_cap_ptr_fsub:
; PURECAP-LIBCALLS:       # %bb.0: # %bb
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, -64
; PURECAP-LIBCALLS-NEXT:    csc cra, 48(csp)
; PURECAP-LIBCALLS-NEXT:    csc cs0, 32(csp)
; PURECAP-LIBCALLS-NEXT:    csc cs1, 16(csp)
; PURECAP-LIBCALLS-NEXT:    cfsd fs0, 8(csp)
; PURECAP-LIBCALLS-NEXT:    fmv.s fs0, fa0
; PURECAP-LIBCALLS-NEXT:    cmove cs0, ca0
; PURECAP-LIBCALLS-NEXT:    cflw fa0, 0(ca0)
; PURECAP-LIBCALLS-NEXT:    addi a0, zero, 4
; PURECAP-LIBCALLS-NEXT:    cincoffset ca1, csp, 4
; PURECAP-LIBCALLS-NEXT:    csetbounds cs1, ca1, a0
; PURECAP-LIBCALLS-NEXT:  .LBB12_1: # %atomicrmw.start
; PURECAP-LIBCALLS-NEXT:    # =>This Inner Loop Header: Depth=1
; PURECAP-LIBCALLS-NEXT:    fsub.s ft0, fa0, fs0
; PURECAP-LIBCALLS-NEXT:    cfsw fa0, 4(csp)
; PURECAP-LIBCALLS-NEXT:    fmv.x.w a2, ft0
; PURECAP-LIBCALLS-NEXT:  .LBB12_3: # %atomicrmw.start
; PURECAP-LIBCALLS-NEXT:    # in Loop: Header=BB12_1 Depth=1
; PURECAP-LIBCALLS-NEXT:    # Label of block must be emitted
; PURECAP-LIBCALLS-NEXT:    auipcc ca5, %captab_pcrel_hi(__atomic_compare_exchange_4)
; PURECAP-LIBCALLS-NEXT:    clc ca5, %pcrel_lo(.LBB12_3)(ca5)
; PURECAP-LIBCALLS-NEXT:    addi a3, zero, 5
; PURECAP-LIBCALLS-NEXT:    addi a4, zero, 5
; PURECAP-LIBCALLS-NEXT:    cmove ca0, cs0
; PURECAP-LIBCALLS-NEXT:    cmove ca1, cs1
; PURECAP-LIBCALLS-NEXT:    cjalr ca5
; PURECAP-LIBCALLS-NEXT:    cflw fa0, 4(csp)
; PURECAP-LIBCALLS-NEXT:    beqz a0, .LBB12_1
; PURECAP-LIBCALLS-NEXT:  # %bb.2: # %atomicrmw.end
; PURECAP-LIBCALLS-NEXT:    cfld fs0, 8(csp)
; PURECAP-LIBCALLS-NEXT:    clc cs1, 16(csp)
; PURECAP-LIBCALLS-NEXT:    clc cs0, 32(csp)
; PURECAP-LIBCALLS-NEXT:    clc cra, 48(csp)
; PURECAP-LIBCALLS-NEXT:    cincoffset csp, csp, 64
; PURECAP-LIBCALLS-NEXT:    cret
bb:
  %tmp = atomicrmw fsub float addrspace(200)* %ptr, float %val seq_cst
  ret float %tmp
}
