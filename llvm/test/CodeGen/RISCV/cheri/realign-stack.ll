; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: %riscv32_cheri_purecap_llc -mattr=+d -verify-machineinstrs -O0 < %s \
; RUN:   | FileCheck --check-prefix=IL32PC64 %s
; RUN: %riscv64_cheri_purecap_llc -mattr=+d -verify-machineinstrs -O0 < %s \
; RUN:   | FileCheck --check-prefix=L64PC128 %s

%struct.wobble = type { [64 x i8] }

define i8 addrspace(200)* @pluto() addrspace(200) nounwind {
; IL32PC64-LABEL: pluto:
; IL32PC64:       # %bb.0: # %bb
; IL32PC64-NEXT:    cincoffset csp, csp, -192
; IL32PC64-NEXT:    csc cra, 184(csp)
; IL32PC64-NEXT:    csc cs0, 176(csp)
; IL32PC64-NEXT:    cincoffset cs0, csp, 192
; IL32PC64-NEXT:    cgetaddr a0, csp
; IL32PC64-NEXT:    andi a0, a0, -64
; IL32PC64-NEXT:    csetaddr csp, csp, a0
; IL32PC64-NEXT:    cincoffset ca0, csp, 64
; IL32PC64-NEXT:    csetbounds ca0, ca0, 64
; IL32PC64-NEXT:    cincoffset csp, cs0, -192
; IL32PC64-NEXT:    clc cs0, 176(csp)
; IL32PC64-NEXT:    clc cra, 184(csp)
; IL32PC64-NEXT:    cincoffset csp, csp, 192
; IL32PC64-NEXT:    cret
;
; L64PC128-LABEL: pluto:
; L64PC128:       # %bb.0: # %bb
; L64PC128-NEXT:    cincoffset csp, csp, -192
; L64PC128-NEXT:    csc cra, 176(csp)
; L64PC128-NEXT:    csc cs0, 160(csp)
; L64PC128-NEXT:    cincoffset cs0, csp, 192
; L64PC128-NEXT:    cgetaddr a0, csp
; L64PC128-NEXT:    andi a0, a0, -64
; L64PC128-NEXT:    csetaddr csp, csp, a0
; L64PC128-NEXT:    cincoffset ca0, csp, 64
; L64PC128-NEXT:    csetbounds ca0, ca0, 64
; L64PC128-NEXT:    cincoffset csp, cs0, -192
; L64PC128-NEXT:    clc cs0, 160(csp)
; L64PC128-NEXT:    clc cra, 176(csp)
; L64PC128-NEXT:    cincoffset csp, csp, 192
; L64PC128-NEXT:    cret
bb:
  %tmp = alloca %struct.wobble, align 64, addrspace(200)
  %ret = bitcast %struct.wobble addrspace(200)* %tmp to i8 addrspace(200)*
  ret i8 addrspace(200)* %ret
}
