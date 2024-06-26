/// In offset mode we should warn about the bitwise operations
// RUN: %cheri_purecap_cc1 -o - -cheri-uintcap=offset -O3 -emit-llvm %s -verify | %cheri_FileCheck '-D$UINTCAP_INTRIN=offset' %s -check-prefixes BROKEN-OPT
/// No need to warn in address mode since the comparision instructions
// RUN: %cheri_purecap_cc1 -o - -cheri-uintcap=addr -O3 -emit-llvm %s -verify=addr-non-exact | %cheri_FileCheck %s '-D$UINTCAP_INTRIN=address' -check-prefixes BROKEN-OPT
// addr-non-exact-no-diagnostics
/// However, this is still a problem with exact equals
// RUN: %cheri_purecap_cc1 -o - -cheri-uintcap=addr -cheri-comparison=exact -O3 -emit-llvm %s -verify | %cheri_FileCheck %s '-D$UINTCAP_INTRIN=address' -check-prefixes BROKEN-OPT

// Now the checks with data-dependent provenance (this is ugly since it breaks the consistent semantic model but does fix this deadlock)
// RUN: %cheri_purecap_cc1 -o - -cheri-uintcap=offset -cheri-data-dependent-provenance -O0 -emit-llvm %s -verify | %cheri_FileCheck '-D$UINTCAP_INTRIN=offset' %s -check-prefixes WORKS
// RUN: %cheri_purecap_cc1 -o - -cheri-uintcap=offset -cheri-data-dependent-provenance -O3 -emit-llvm %s -verify | %cheri_FileCheck '-D$UINTCAP_INTRIN=offset' %s -check-prefixes WORKS-OPT
// RUN: %cheri_purecap_cc1 -o - -cheri-uintcap=addr -cheri-data-dependent-provenance -O0 -emit-llvm %s -verify=datadep-addr | %cheri_FileCheck '-D$UINTCAP_INTRIN=address' %s -check-prefixes WORKS
// RUN: %cheri_purecap_cc1 -o - -cheri-uintcap=addr -cheri-data-dependent-provenance -O3 -emit-llvm %s -verify=datadep-addr | %cheri_FileCheck '-D$UINTCAP_INTRIN=address' %s -check-prefixes WORKS-OPT
// datadep-addr-no-diagnostics


void do_unlock(void);
__uintcap_t this_broke_qmutex(__uintcap_t mtx) {

  // This is a slightly adapted version of the code used in QMutexLocker():
  //    inline void unlock() Q_DECL_NOTHROW
  //    {
  //        if ((val & quintptr(1u)) == quintptr(1u)) {
  //            val &= ~quintptr(1u);
  //            mutex()->unlock();
  //        }
  //    }
  // By casting to uintptr_t Qt silences warnings but this causes an issue for the current CHERI C implementation:
  // The LHS of the equality comparision will be a capability derived from val, but with it's address/offset changed
  // to be anded with 1. Comparing this to a null-derived capability with offset 1 will always return false in the
  // offset interpretation and in the address interpretation it will also be always false if we enforce exact equals
  // instead of only comparing the vaddr.
  // The is no good fix for this since when checking if the low bit is set we want derivation from NULL, but when
  // then clearing the locked flag we do want to keep provenance. This means we can't have a consistent sematic model
  // without breaking one of the cases. The previous solution was to just always warn on bitwise-and but this is really
  // noisy and therefore will not be feasible if we want wide-spread adoption of CHERI for legacy codebases.
  //
  // By using a data-dependent provenance we can make this work for both cases:
  // - if the value is a small integer we derive from NULL (e.g. less than 1 page since those values should not be dereferenceable)
  // - otherwise we derive from the LHS and keep provenance
  //
  // This should not be a problem in most cases since the value will generally be a compile-time constant so the
  // LLVM optimizer can eliminate the select.
  if ((mtx & (__uintcap_t)1) == (__uintcap_t)1) { // locked flag is set
    // expected-warning@-1{{using bitwise and on a capability type may give surprising results}}
    mtx &= ~1;                                    // clear locked flag flag
    // expected-warning@-1{{using bitwise and on a capability type may give surprising results}}
    do_unlock();
  }
  return mtx;
}

// BROKEN-OPT-LABEL: @this_broke_qmutex(
// BROKEN-OPT-NEXT:  entry:
// BROKEN-OPT-NEXT:    [[TMP1:%.*]] = tail call i64 @llvm.cheri.cap.[[$UINTCAP_INTRIN]].get.i64(ptr addrspace(200) [[MTX:%.*]])
// BROKEN-OPT-NEXT:    [[AND:%.*]] = and i64 [[TMP1]], 1
// BROKEN-OPT-NEXT:    [[TMP2:%.*]] = tail call ptr addrspace(200) @llvm.cheri.cap.[[$UINTCAP_INTRIN]].set.i64(ptr addrspace(200) [[MTX]], i64 [[AND]])
// BROKEN-OPT-NEXT:    [[CMP:%.*]] = icmp eq ptr addrspace(200) [[TMP2]], getelementptr (i8, ptr addrspace(200) null, i64 1)
// BROKEN-OPT-NEXT:    br i1 [[CMP]], label [[IF_THEN:%.*]], label [[IF_END:%.*]]
// BROKEN-OPT:       if.then:
// BROKEN-OPT-NEXT:    [[AND1:%.*]] = and i64 [[TMP1]], -2
// BROKEN-OPT-NEXT:    [[TMP3:%.*]] = tail call ptr addrspace(200) @llvm.cheri.cap.[[$UINTCAP_INTRIN]].set.i64(ptr addrspace(200) [[MTX]], i64 [[AND1]])
// BROKEN-OPT-NEXT:    tail call void @do_unlock() #3
// BROKEN-OPT-NEXT:    br label [[IF_END]]
// BROKEN-OPT:       if.end:
// BROKEN-OPT-NEXT:    [[MTX_ADDR_0:%.*]] = phi ptr addrspace(200) [ [[TMP3]], [[IF_THEN]] ], [ [[MTX]], [[ENTRY:%.*]] ]
// BROKEN-OPT-NEXT:    ret ptr addrspace(200) [[MTX_ADDR_0]]
//
// WORKS-LABEL: @this_broke_qmutex(
// WORKS-NEXT:  entry:
// WORKS-NEXT:    [[MTX_ADDR:%.*]] = alloca ptr addrspace(200), align [[#CAP_SIZE]], addrspace(200)
// WORKS-NEXT:    store ptr addrspace(200) [[MTX:%.*]], ptr addrspace(200) [[MTX_ADDR]], align [[#CAP_SIZE]]
// WORKS-NEXT:    [[TMP0:%.*]] = load ptr addrspace(200), ptr addrspace(200) [[MTX_ADDR]], align [[#CAP_SIZE]]
// WORKS-NEXT:    [[TMP2:%.*]] = call i64 @llvm.cheri.cap.[[$UINTCAP_INTRIN]].get.i64(ptr addrspace(200) [[TMP0]])
// WORKS-NEXT:    [[TMP3:%.*]] = call i64 @llvm.cheri.cap.[[$UINTCAP_INTRIN]].get.i64(ptr addrspace(200) getelementptr (i8, ptr addrspace(200) null, i64 1))
// WORKS-NEXT:    [[AND:%.*]] = and i64 [[TMP2]], [[TMP3]]
// WORKS-NEXT:    [[BITAND_SHOULD_NULLDERIVE:%.*]] = icmp ule i64 [[TMP3]], 4096
// WORKS-NEXT:    [[BITAND_PROVENANCE:%.*]] = select i1 [[BITAND_SHOULD_NULLDERIVE]], ptr addrspace(200) null, ptr addrspace(200) [[TMP0]]
// WORKS-NEXT:    [[TMP4:%.*]] = call ptr addrspace(200) @llvm.cheri.cap.[[$UINTCAP_INTRIN]].set.i64(ptr addrspace(200) [[BITAND_PROVENANCE]], i64 [[AND]])
// WORKS-NEXT:    [[CMP:%.*]] = icmp eq ptr addrspace(200) [[TMP4]], getelementptr (i8, ptr addrspace(200) null, i64 1)
// WORKS-NEXT:    br i1 [[CMP]], label [[IF_THEN:%.*]], label [[IF_END:%.*]]
// WORKS:       if.then:
// WORKS-NEXT:    [[TMP7:%.*]] = load ptr addrspace(200), ptr addrspace(200) [[MTX_ADDR]], align [[#CAP_SIZE]]
// WORKS-NEXT:    [[TMP8:%.*]] = call i64 @llvm.cheri.cap.[[$UINTCAP_INTRIN]].get.i64(ptr addrspace(200) [[TMP7]])
// WORKS-NEXT:    [[TMP9:%.*]] = call i64 @llvm.cheri.cap.[[$UINTCAP_INTRIN]].get.i64(ptr addrspace(200) getelementptr (i8, ptr addrspace(200) null, i64 -2))
// WORKS-NEXT:    [[AND1:%.*]] = and i64 [[TMP8]], [[TMP9]]
// WORKS-NEXT:    [[BITAND_SHOULD_NULLDERIVE2:%.*]] = icmp ule i64 [[TMP9]], 4096
// WORKS-NEXT:    [[BITAND_PROVENANCE3:%.*]] = select i1 [[BITAND_SHOULD_NULLDERIVE2]], ptr addrspace(200) null, ptr addrspace(200) [[TMP7]]
// WORKS-NEXT:    [[TMP10:%.*]] = call ptr addrspace(200) @llvm.cheri.cap.[[$UINTCAP_INTRIN]].set.i64(ptr addrspace(200) [[BITAND_PROVENANCE3]], i64 [[AND1]])
// WORKS-NEXT:    store ptr addrspace(200) [[TMP10]], ptr addrspace(200) [[MTX_ADDR]], align [[#CAP_SIZE]]
// WORKS-NEXT:    call void @do_unlock()
// WORKS-NEXT:    br label [[IF_END]]
// WORKS:       if.end:
// WORKS-NEXT:    [[TMP11:%.*]] = load ptr addrspace(200), ptr addrspace(200) [[MTX_ADDR]], align [[#CAP_SIZE]]
// WORKS-NEXT:    ret ptr addrspace(200) [[TMP11]]
//
// WORKS-OPT-LABEL: @this_broke_qmutex(
// WORKS-OPT-NEXT:  entry:
// WORKS-OPT-NEXT:    [[TMP:%.*]] = tail call i64 @llvm.cheri.cap.[[$UINTCAP_INTRIN]].get.i64(ptr addrspace(200) [[MTX:%.*]])
// WORKS-OPT-NEXT:    [[AND:%.*]] = and i64 [[TMP]], 1
// WORKS-OPT-NEXT:    [[CMP:%.*]] = icmp eq i64 [[AND]], 0
// WORKS-OPT-NEXT:    br i1 [[CMP]], label [[IF_END:%.*]], label [[IF_THEN:%.*]]
// WORKS-OPT:       if.then:
// WORKS-OPT-NEXT:    [[AND1:%.*]] = and i64 [[TMP]], -2
// WORKS-OPT-NEXT:    [[TMP3:%.*]] = tail call ptr addrspace(200) @llvm.cheri.cap.[[$UINTCAP_INTRIN]].set.i64(ptr addrspace(200) [[MTX]], i64 [[AND1]])
// WORKS-OPT-NEXT:    tail call void @do_unlock() #3
// WORKS-OPT-NEXT:    br label [[IF_END]]
// WORKS-OPT:       if.end:
// WORKS-OPT-NEXT:    [[MTX_ADDR_0:%.*]] = phi ptr addrspace(200) [ [[TMP3]], [[IF_THEN]] ], [ [[MTX]], [[ENTRY:%.*]] ]
// WORKS-OPT-NEXT:    ret ptr addrspace(200) [[MTX_ADDR_0]]
//

// Same thing but not optimizable
// TODO: should we diagnose these cases where we actually emit a runtime check?
// BROKEN-OPT-LABEL: @can_fold_the_bitand_provenance_check(
// BROKEN-OPT-NEXT:  entry:
// BROKEN-OPT-NEXT:    [[TMP1:%.*]] = tail call i64 @llvm.cheri.cap.[[$UINTCAP_INTRIN]].get.i64(ptr addrspace(200) [[MTX:%.*]])
// BROKEN-OPT-NEXT:    [[AND:%.*]] = and i64 [[TMP1]], 1
// BROKEN-OPT-NEXT:    [[TMP2:%.*]] = tail call ptr addrspace(200) @llvm.cheri.cap.[[$UINTCAP_INTRIN]].set.i64(ptr addrspace(200) [[MTX]], i64 [[AND]])
// BROKEN-OPT-NEXT:    [[CMP:%.*]] = icmp eq ptr addrspace(200) [[TMP2]], getelementptr (i8, ptr addrspace(200) null, i64 1)
// BROKEN-OPT-NEXT:    br i1 [[CMP]], label [[IF_THEN:%.*]], label [[IF_END:%.*]]
// BROKEN-OPT:       if.then:
// BROKEN-OPT-NEXT:    [[AND1:%.*]] = and i64 [[TMP1]], -2
// BROKEN-OPT-NEXT:    [[TMP3:%.*]] = tail call ptr addrspace(200) @llvm.cheri.cap.[[$UINTCAP_INTRIN]].set.i64(ptr addrspace(200) [[MTX]], i64 [[AND1]])
// BROKEN-OPT-NEXT:    tail call void @do_unlock() #3
// BROKEN-OPT-NEXT:    br label [[IF_END]]
// BROKEN-OPT:       if.end:
// BROKEN-OPT-NEXT:    [[MTX_ADDR_0:%.*]] = phi ptr addrspace(200) [ [[TMP3]], [[IF_THEN]] ], [ [[MTX]], [[ENTRY:%.*]] ]
// BROKEN-OPT-NEXT:    ret ptr addrspace(200) [[MTX_ADDR_0]]
//
// WORKS-LABEL: @can_fold_the_bitand_provenance_check(
// WORKS-NEXT:  entry:
// WORKS-NEXT:    [[MTX_ADDR:%.*]] = alloca ptr addrspace(200), align [[#CAP_SIZE]], addrspace(200)
// WORKS-NEXT:    store ptr addrspace(200) [[MTX:%.*]], ptr addrspace(200) [[MTX_ADDR]], align [[#CAP_SIZE]]
// WORKS-NEXT:    [[TMP0:%.*]] = load ptr addrspace(200), ptr addrspace(200) [[MTX_ADDR]], align [[#CAP_SIZE]]
// WORKS-NEXT:    [[TMP2:%.*]] = call i64 @llvm.cheri.cap.[[$UINTCAP_INTRIN]].get.i64(ptr addrspace(200) [[TMP0]])
// WORKS-NEXT:    [[TMP3:%.*]] = call i64 @llvm.cheri.cap.[[$UINTCAP_INTRIN]].get.i64(ptr addrspace(200) getelementptr (i8, ptr addrspace(200) null, i64 1))
// WORKS-NEXT:    [[AND:%.*]] = and i64 [[TMP2]], [[TMP3]]
// WORKS-NEXT:    [[BITAND_SHOULD_NULLDERIVE:%.*]] = icmp ule i64 [[TMP3]], 4096
// WORKS-NEXT:    [[BITAND_PROVENANCE:%.*]] = select i1 [[BITAND_SHOULD_NULLDERIVE]], ptr addrspace(200) null, ptr addrspace(200) [[TMP0]]
// WORKS-NEXT:    [[TMP4:%.*]] = call ptr addrspace(200) @llvm.cheri.cap.[[$UINTCAP_INTRIN]].set.i64(ptr addrspace(200) [[BITAND_PROVENANCE]], i64 [[AND]])
// WORKS-NEXT:    [[CMP:%.*]] = icmp eq ptr addrspace(200) [[TMP4]], getelementptr (i8, ptr addrspace(200) null, i64 1)
// WORKS-NEXT:    br i1 [[CMP]], label [[IF_THEN:%.*]], label [[IF_END:%.*]]
// WORKS:       if.then:
// WORKS-NEXT:    [[TMP7:%.*]] = load ptr addrspace(200), ptr addrspace(200) [[MTX_ADDR]], align [[#CAP_SIZE]]
// WORKS-NEXT:    [[TMP8:%.*]] = call i64 @llvm.cheri.cap.[[$UINTCAP_INTRIN]].get.i64(ptr addrspace(200) [[TMP7]])
// WORKS-NEXT:    [[TMP9:%.*]] = call i64 @llvm.cheri.cap.[[$UINTCAP_INTRIN]].get.i64(ptr addrspace(200) getelementptr (i8, ptr addrspace(200) null, i64 -2))
// WORKS-NEXT:    [[AND1:%.*]] = and i64 [[TMP8]], [[TMP9]]
// WORKS-NEXT:    [[BITAND_SHOULD_NULLDERIVE2:%.*]] = icmp ule i64 [[TMP9]], 4096
// WORKS-NEXT:    [[BITAND_PROVENANCE3:%.*]] = select i1 [[BITAND_SHOULD_NULLDERIVE2]], ptr addrspace(200) null, ptr addrspace(200) [[TMP7]]
// WORKS-NEXT:    [[TMP10:%.*]] = call ptr addrspace(200) @llvm.cheri.cap.[[$UINTCAP_INTRIN]].set.i64(ptr addrspace(200) [[BITAND_PROVENANCE3]], i64 [[AND1]])
// WORKS-NEXT:    store ptr addrspace(200) [[TMP10]], ptr addrspace(200) [[MTX_ADDR]], align [[#CAP_SIZE]]
// WORKS-NEXT:    call void @do_unlock()
// WORKS-NEXT:    br label [[IF_END]]
// WORKS:       if.end:
// WORKS-NEXT:    [[TMP11:%.*]] = load ptr addrspace(200), ptr addrspace(200) [[MTX_ADDR]], align [[#CAP_SIZE]]
// WORKS-NEXT:    ret ptr addrspace(200) [[TMP11]]
//
// WORKS-OPT-LABEL: @can_fold_the_bitand_provenance_check(
// WORKS-OPT-NEXT:  entry:
// WORKS-OPT-NEXT:    [[TMP:%.*]] = tail call i64 @llvm.cheri.cap.[[$UINTCAP_INTRIN]].get.i64(ptr addrspace(200) [[MTX:%.*]])
// WORKS-OPT-NEXT:    [[AND:%.*]] = and i64 [[TMP]], 1
// WORKS-OPT-NEXT:    [[CMP:%.*]] = icmp eq i64 [[AND]], 0
// WORKS-OPT-NEXT:    br i1 [[CMP]], label [[IF_END:%.*]], label [[IF_THEN:%.*]]
// WORKS-OPT:       if.then:
// WORKS-OPT-NEXT:    [[AND1:%.*]] = and i64 [[TMP]], -2
// WORKS-OPT-NEXT:    [[TMP3:%.*]] = tail call ptr addrspace(200) @llvm.cheri.cap.[[$UINTCAP_INTRIN]].set.i64(ptr addrspace(200) [[MTX]], i64 [[AND1]])
// WORKS-OPT-NEXT:    tail call void @do_unlock() #3
// WORKS-OPT-NEXT:    br label [[IF_END]]
// WORKS-OPT:       if.end:
// WORKS-OPT-NEXT:    [[MTX_ADDR_0:%.*]] = phi ptr addrspace(200) [ [[TMP3]], [[IF_THEN]] ], [ [[MTX]], [[ENTRY:%.*]] ]
// WORKS-OPT-NEXT:    ret ptr addrspace(200) [[MTX_ADDR_0]]
//
__uintcap_t can_fold_the_bitand_provenance_check(__uintcap_t mtx) {
  if ((mtx & (__uintcap_t)1) == (__uintcap_t)1) {
    // expected-warning@-1{{using bitwise and on a capability type may give surprising results}}
    mtx &= ~1;
    // expected-warning@-1{{using bitwise and on a capability type may give surprising results}}
    do_unlock();
  }
  return mtx;
}
