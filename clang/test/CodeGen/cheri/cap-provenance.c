// Check that we can infer the correct provenance source

// RUN: %cheri_purecap_cc1 %s -emit-llvm -o - -DBITOR -Wcheri -verify -O1 | FileCheck %s -check-prefixes=CHECK,NOTADD '-D$ARITH_OP=or'
// RUN: %cheri_purecap_cc1 %s -emit-llvm -o - -DBITAND -Wcheri -verify -O1 | FileCheck %s -check-prefixes=CHECK,NOTADD '-D$ARITH_OP=and'
// RUN: %cheri_purecap_cc1 %s -emit-llvm -o - -DBITXOR -Wcheri -verify -O1 | FileCheck %s -check-prefixes=CHECK,NOTADD '-D$ARITH_OP=xor'
// RUN: %cheri_purecap_cc1 %s -emit-llvm -o - -DMULT -Wcheri -verify -O1 | FileCheck %s -check-prefixes=CHECK,NOTADD '-D$ARITH_OP=mul'
// RUN: %cheri_purecap_cc1 %s -emit-llvm -o - -DADD -Wcheri -verify -O1 | FileCheck %s -check-prefixes=CHECK,ADD
// RUN: %cheri_cc1 %s -emit-llvm -o - -DBITOR -Wcheri -verify=hybrid -O1

#pragma clang diagnostic ignored "-Wcheri-bitwise-operations"

typedef __UINTPTR_TYPE__ uintcap_t;
typedef uintcap_t uintptr_t;
#define __no_provenance __attribute__((cheri_no_provenance))
typedef __no_provenance uintcap_t no_provenance_uintptr_t; // hybrid-error{{'cheri_no_provenance' attribute only applies to capability types}}
typedef unsigned int uint;
typedef unsigned long ulong;

#ifdef ADD
#define ARITH_OP +
#define ARITH_EQ_OP +=
#elif defined(MULT)
#define ARITH_OP *
#define ARITH_EQ_OP *=
#elif defined(BITOR)
#define ARITH_OP |
#define ARITH_EQ_OP |=
#elif defined(BITAND)
#define ARITH_OP &
#define ARITH_EQ_OP &=
#elif defined(BITXOR)
#define ARITH_OP ^
#define ARITH_EQ_OP ^=
#else
#error INVALID
#endif

void check(uintptr_t arg);

// CHECK-LABEL: define {{[^@]+}}@test_cg_prov_lhs
// CHECK-SAME: (ptr addrspace(200) noundef [[LHS:%.*]], ptr addrspace(200) noundef [[RHS:%.*]]) local_unnamed_addr addrspace(200)
// CHECK-NEXT:  entry:
// NOTADD-NEXT:   [[LHS_ADDR:%.*]] = tail call i64 @llvm.cheri.cap.address.get.i64(ptr addrspace(200) [[LHS]])
// CHECK-NEXT:    [[RHS_ADDR:%.*]] = tail call i64 @llvm.cheri.cap.address.get.i64(ptr addrspace(200) [[RHS]])
// NOTADD-NEXT:   [[ARITH_RESULT:%.*]] = [[$ARITH_OP]] i64 [[RHS_ADDR]], [[LHS_ADDR]]
// ADD-NEXT:      [[RESULT:%.*]] = getelementptr i8, ptr addrspace(200) [[LHS]], i64 [[RHS_ADDR]]
// NOTADD-NEXT:   [[RESULT:%.*]] = tail call ptr addrspace(200) @llvm.cheri.cap.address.set.i64(ptr addrspace(200) [[LHS]], i64 [[ARITH_RESULT]])
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef [[RESULT]]) #4
// CHECK-NEXT:    ret void
//
void test_cg_prov_lhs(uintptr_t lhs, uintptr_t rhs) {
  check(lhs ARITH_OP(__no_provenance uintptr_t) rhs); // no warning, LHS provenance
  // hybrid-error@-1{{'cheri_no_provenance' attribute only applies to capability types}}
}

// CHECK-LABEL: define {{[^@]+}}@test_cg_prov_rhs
// CHECK-SAME: (ptr addrspace(200) noundef [[LHS:%.*]], ptr addrspace(200) noundef [[RHS:%.*]]) local_unnamed_addr addrspace(200)
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[LHS_ADDR:%.*]] = tail call i64 @llvm.cheri.cap.address.get.i64(ptr addrspace(200) [[LHS]])
// NOTADD-NEXT:   [[RHS_ADDR:%.*]] = tail call i64 @llvm.cheri.cap.address.get.i64(ptr addrspace(200) [[RHS]])
// NOTADD-NEXT:   [[ARITH_RESULT:%.*]] = [[$ARITH_OP]] i64 [[RHS_ADDR]], [[LHS_ADDR]]
// ADD-NEXT:      [[RESULT:%.*]] = getelementptr i8, ptr addrspace(200) [[RHS]], i64 [[LHS_ADDR]]
// NOTADD-NEXT:   [[RESULT:%.*]] = tail call ptr addrspace(200) @llvm.cheri.cap.address.set.i64(ptr addrspace(200) [[RHS]], i64 [[ARITH_RESULT]])
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef [[RESULT]]) #4
// CHECK-NEXT:    ret void
//
void test_cg_prov_rhs(uintptr_t lhs, uintptr_t rhs) {
  check((__no_provenance uintptr_t)lhs ARITH_OP rhs); // no warning, RHS provenance
  // hybrid-error@-1{{'cheri_no_provenance' attribute only applies to capability types}}
}

// CHECK-LABEL: define {{[^@]+}}@test_cg_prov_ambiguous
// CHECK-SAME: (ptr addrspace(200) noundef [[LHS:%.*]], ptr addrspace(200) noundef [[RHS:%.*]]) local_unnamed_addr addrspace(200)
// CHECK-NEXT:  entry:
// NOTADD-NEXT:   [[LHS_ADDR:%.*]] = tail call i64 @llvm.cheri.cap.address.get.i64(ptr addrspace(200) [[LHS]])
// CHECK-NEXT:    [[RHS_ADDR:%.*]] = tail call i64 @llvm.cheri.cap.address.get.i64(ptr addrspace(200) [[RHS]])
// NOTADD-NEXT:   [[ARITH_RESULT:%.*]] = [[$ARITH_OP]] i64 [[RHS_ADDR]], [[LHS_ADDR]]
// ADD-NEXT:      [[RESULT:%.*]] = getelementptr i8, ptr addrspace(200) [[LHS]], i64 [[RHS_ADDR]]
// NOTADD-NEXT:   [[RESULT:%.*]] = tail call ptr addrspace(200) @llvm.cheri.cap.address.set.i64(ptr addrspace(200) [[LHS]], i64 [[ARITH_RESULT]])
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef [[RESULT]]) #4
// CHECK-NEXT:    ret void
//
void test_cg_prov_ambiguous(uintptr_t lhs, uintptr_t rhs) {
  // LHS provenance is chosen when ambiguous
  check(lhs ARITH_OP rhs); // expected-warning{{binary expression on capability types 'uintptr_t' (aka 'unsigned __intcap') and 'uintptr_t'; it is not clear which should be used as the source of provenance}}
}

/// Compound assignment should always use LHS provenance:
// CHECK-LABEL: define {{[^@]+}}@test_cg_eq_op_ambiguous
// CHECK-SAME: (ptr addrspace(200) noundef{{( readnone)?}} [[LHS:%.*]], ptr addrspace(200) noundef{{( readnone)?}} [[RHS:%.*]]) local_unnamed_addr addrspace(200)
// CHECK-NEXT:  entry:
// NOTADD-NEXT:   [[LHS_ADDR:%.*]] = tail call i64 @llvm.cheri.cap.address.get.i64(ptr addrspace(200) [[LHS]])
// CHECK-NEXT:    [[RHS_ADDR:%.*]] = tail call i64 @llvm.cheri.cap.address.get.i64(ptr addrspace(200) [[RHS]])
// NOTADD-NEXT:   [[ARITH_RESULT:%.*]] = [[$ARITH_OP]] i64 [[RHS_ADDR]], [[LHS_ADDR]]
// NOTADD-NEXT:   [[RESULT:%.*]] = tail call ptr addrspace(200) @llvm.cheri.cap.address.set.i64(ptr addrspace(200) [[LHS]], i64 [[ARITH_RESULT]])
// ADD-NEXT:      [[RESULT:%.*]] = getelementptr i8, ptr addrspace(200) [[LHS]], i64 [[RHS_ADDR]]
// CHECK-NEXT:    ret ptr addrspace(200) [[RESULT]]
//
uintptr_t test_cg_eq_op_ambiguous(uintptr_t lhs, uintptr_t rhs) {
  // +=/|=, etc uses LHS (but should warn)
  lhs ARITH_EQ_OP rhs;
  return lhs;
}

/// Compound assignment should always use LHS provenance:
// CHECK-LABEL: define {{[^@]+}}@test_cg_eq_op_lhs_noprov
// CHECK-SAME: (ptr addrspace(200) noundef{{( readnone)?}} [[LHS:%.*]], ptr addrspace(200) noundef{{( readnone)?}} [[RHS:%.*]]) local_unnamed_addr addrspace(200)
// CHECK-NEXT:  entry:
// NOTADD-NEXT:   [[LHS_ADDR:%.*]] = tail call i64 @llvm.cheri.cap.address.get.i64(ptr addrspace(200) [[LHS]])
// CHECK-NEXT:    [[RHS_ADDR:%.*]] = tail call i64 @llvm.cheri.cap.address.get.i64(ptr addrspace(200) [[RHS]])
// NOTADD-NEXT:   [[ARITH_RESULT:%.*]] = [[$ARITH_OP]] i64 [[RHS_ADDR]], [[LHS_ADDR]]
// NOTADD-NEXT:   [[RESULT:%.*]] = tail call ptr addrspace(200) @llvm.cheri.cap.address.set.i64(ptr addrspace(200) [[LHS]], i64 [[ARITH_RESULT]])
// ADD-NEXT:      [[RESULT:%.*]] = getelementptr i8, ptr addrspace(200) [[LHS]], i64 [[RHS_ADDR]]
// CHECK-NEXT:    ret ptr addrspace(200) [[RESULT]]
//
uintptr_t test_cg_eq_op_lhs_noprov(no_provenance_uintptr_t lhs, uintptr_t rhs) {
  lhs ARITH_EQ_OP rhs; // FIXME: this will strip provenance from rhs -> warn?
  return lhs;
}

/// Compound assignment should always use LHS provenance:
// CHECK-LABEL: define {{[^@]+}}@test_cg_eq_op_rhs_noprov
// CHECK-SAME: (ptr addrspace(200) noundef{{( readnone)?}} [[LHS:%.*]], ptr addrspace(200) noundef{{( readnone)?}} [[RHS:%.*]]) local_unnamed_addr addrspace(200)
// CHECK-NEXT:  entry:
// NOTADD-NEXT:   [[LHS_ADDR:%.*]] = tail call i64 @llvm.cheri.cap.address.get.i64(ptr addrspace(200) [[LHS]])
// CHECK-NEXT:    [[RHS_ADDR:%.*]] = tail call i64 @llvm.cheri.cap.address.get.i64(ptr addrspace(200) [[RHS]])
// NOTADD-NEXT:   [[ARITH_RESULT:%.*]] = [[$ARITH_OP]] i64 [[RHS_ADDR]], [[LHS_ADDR]]
// NOTADD-NEXT:   [[RESULT:%.*]] = tail call ptr addrspace(200) @llvm.cheri.cap.address.set.i64(ptr addrspace(200) [[LHS]], i64 [[ARITH_RESULT]])
// ADD-NEXT:      [[RESULT:%.*]] = getelementptr i8, ptr addrspace(200) [[LHS]], i64 [[RHS_ADDR]]
// CHECK-NEXT:    ret ptr addrspace(200) [[RESULT]]
//
uintptr_t test_cg_eq_op_rhs_noprov(uintptr_t lhs, no_provenance_uintptr_t rhs) {
  lhs ARITH_EQ_OP rhs; // fine
  return lhs;
}

// CHECK-LABEL: define {{[^@]+}}@test_cg_unary_minus
// CHECK-SAME: (ptr addrspace(200) noundef [[ARG:%.*]], ptr addrspace(200) noundef [[ARG_PTR:%.*]]) local_unnamed_addr addrspace(200)
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[TMP0:%.*]] = tail call i64 @llvm.cheri.cap.address.get.i64(ptr addrspace(200) [[ARG:%.*]])
// CHECK-NEXT:    [[SUB:%.*]] = sub i64 0, [[TMP0]]
// CHECK-NEXT:    [[TMP1:%.*]] = tail call ptr addrspace(200) @llvm.cheri.cap.address.set.i64(ptr addrspace(200) [[ARG]], i64 [[SUB]])
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef [[TMP1]]) #4
// CHECK-NEXT:    [[TMP2:%.*]] = tail call i64 @llvm.cheri.cap.address.get.i64(ptr addrspace(200) [[ARG_PTR:%.*]])
// CHECK-NEXT:    [[SUB1:%.*]] = sub i64 0, [[TMP2]]
// CHECK-NEXT:    [[TMP3:%.*]] = tail call ptr addrspace(200) @llvm.cheri.cap.address.set.i64(ptr addrspace(200) [[ARG_PTR]], i64 [[SUB1]])
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef [[TMP3]]) #4
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef getelementptr (i8, ptr addrspace(200) null, i64 -5)) #4
// CHECK-NEXT:    ret void
//
void test_cg_unary_minus(uintptr_t arg, char *arg_ptr) {
  check(-arg);
  check(-(uintptr_t)arg_ptr);
  uintptr_t x = 5;
  check(-x);
}

// CHECK-LABEL: define {{[^@]+}}@test_cg_unary_not
// CHECK-SAME: (ptr addrspace(200) noundef [[ARG:%.*]], ptr addrspace(200) noundef [[ARG_PTR:%.*]]) local_unnamed_addr addrspace(200)
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[TMP0:%.*]] = tail call i64 @llvm.cheri.cap.address.get.i64(ptr addrspace(200) [[ARG:%.*]])
// CHECK-NEXT:    [[NEG:%.*]] = xor i64 [[TMP0]], -1
// CHECK-NEXT:    [[TMP1:%.*]] = tail call ptr addrspace(200) @llvm.cheri.cap.address.set.i64(ptr addrspace(200) [[ARG]], i64 [[NEG]])
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef [[TMP1]]) #4
// CHECK-NEXT:    [[TMP2:%.*]] = tail call i64 @llvm.cheri.cap.address.get.i64(ptr addrspace(200) [[ARG_PTR:%.*]])
// CHECK-NEXT:    [[NEG1:%.*]] = xor i64 [[TMP2]], -1
// CHECK-NEXT:    [[TMP3:%.*]] = tail call ptr addrspace(200) @llvm.cheri.cap.address.set.i64(ptr addrspace(200) [[ARG_PTR]], i64 [[NEG1]])
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef [[TMP3]]) #4
// CHECK-NEXT:    ret void
//
void test_cg_unary_not(uintptr_t arg, char *arg_ptr) {
  check(~arg);
  check(~(uintptr_t)arg_ptr);
}

// CHECK-LABEL: define {{[^@]+}}@test_cg_unary_logical_not
// CHECK-SAME: (ptr addrspace(200) noundef readnone [[ARG:%.*]], ptr addrspace(200) noundef readnone [[ARG_PTR:%.*]]) local_unnamed_addr addrspace(200)
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[TOBOOL:%.*]] = icmp eq ptr addrspace(200) [[ARG:%.*]], null
// CHECK-NEXT:    [[CONV:%.*]] = zext i1 [[TOBOOL]] to i64
// CHECK-NEXT:    [[TMP0:%.*]] = getelementptr i8, ptr addrspace(200) null, i64 [[CONV]]
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef [[TMP0]]) #4
// CHECK-NEXT:    [[TOBOOL1:%.*]] = icmp eq ptr addrspace(200) [[ARG_PTR:%.*]], null
// CHECK-NEXT:    [[CONV4:%.*]] = zext i1 [[TOBOOL1]] to i64
// CHECK-NEXT:    [[TMP1:%.*]] = getelementptr i8, ptr addrspace(200) null, i64 [[CONV4]]
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef [[TMP1]]) #4
// CHECK-NEXT:    ret void
//
void test_cg_unary_logical_not(uintptr_t arg, char *arg_ptr) {
  check(!arg);
  check(!(uintptr_t)arg_ptr);
}

// CHECK-LABEL: define {{[^@]+}}@test_cg_unary_plus
// CHECK-SAME: (ptr addrspace(200) noundef [[ARG:%.*]], ptr addrspace(200) noundef [[ARG_PTR:%.*]]) local_unnamed_addr addrspace(200)
// CHECK-NEXT:  entry:
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef [[ARG:%.*]]) #4
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef [[ARG_PTR:%.*]]) #4
// CHECK-NEXT:    ret void
//
void test_cg_unary_plus(uintptr_t arg, char *arg_ptr) {
  check(+arg);
  check(+(uintptr_t)arg_ptr);
}

// CHECK-LABEL: define {{[^@]+}}@test_cg_preinc
// CHECK-SAME: (ptr addrspace(200) noundef [[ARG:%.*]], ptr addrspace(200) noundef [[ARG_PTR:%.*]]) local_unnamed_addr addrspace(200)
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[TMP0:%.*]] = getelementptr i8, ptr addrspace(200) [[ARG:%.*]], i64 1
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef [[TMP0]]) #4
// CHECK-NEXT:    [[INCDEC_PTR:%.*]] = getelementptr inbounds i8, ptr addrspace(200) [[ARG_PTR:%.*]], i64 1
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef nonnull [[INCDEC_PTR]]) #4
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef [[TMP0]]) #4
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef nonnull [[INCDEC_PTR]]) #4
// CHECK-NEXT:    ret void
//
void test_cg_preinc(uintptr_t arg, char *arg_ptr) {
  check(++arg);
  check((uintptr_t)++arg_ptr);
  check(arg);
  check((uintptr_t)arg_ptr);
}

// CHECK-LABEL: define {{[^@]+}}@test_cg_postinc
// CHECK-SAME: (ptr addrspace(200) noundef [[ARG:%.*]], ptr addrspace(200) noundef [[ARG_PTR:%.*]]) local_unnamed_addr addrspace(200)
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[TMP0:%.*]] = getelementptr i8, ptr addrspace(200) [[ARG:%.*]], i64 1
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef [[ARG]]) #4
// CHECK-NEXT:    [[INCDEC_PTR:%.*]] = getelementptr inbounds i8, ptr addrspace(200) [[ARG_PTR:%.*]], i64 1
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef [[ARG_PTR]]) #4
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef [[TMP0]]) #4
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef nonnull [[INCDEC_PTR]]) #4
// CHECK-NEXT:    ret void
//
void test_cg_postinc(uintptr_t arg, char *arg_ptr) {
  check(arg++);
  check((uintptr_t)arg_ptr++);
  check(arg);
  check((uintptr_t)arg_ptr);
}

// CHECK-LABEL: define {{[^@]+}}@test_cg_predec
// CHECK-SAME: (ptr addrspace(200) noundef [[ARG:%.*]], ptr addrspace(200) noundef [[ARG_PTR:%.*]]) local_unnamed_addr addrspace(200)
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[TMP0:%.*]] = getelementptr i8, ptr addrspace(200) [[ARG:%.*]], i64 -1
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef [[TMP0]]) #4
// CHECK-NEXT:    [[INCDEC_PTR:%.*]] = getelementptr inbounds i8, ptr addrspace(200) [[ARG_PTR:%.*]], i64 -1
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef nonnull [[INCDEC_PTR]]) #4
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef [[TMP0]]) #4
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef nonnull [[INCDEC_PTR]]) #4
// CHECK-NEXT:    ret void
//
void test_cg_predec(uintptr_t arg, char *arg_ptr) {
  check(--arg);
  check((uintptr_t)--arg_ptr);
  check(arg);
  check((uintptr_t)arg_ptr);
}

// CHECK-LABEL: define {{[^@]+}}@test_cg_postdec
// CHECK-SAME: (ptr addrspace(200) noundef [[ARG:%.*]], ptr addrspace(200) noundef [[ARG_PTR:%.*]]) local_unnamed_addr addrspace(200)
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[TMP0:%.*]] = getelementptr i8, ptr addrspace(200) [[ARG:%.*]], i64 -1
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef [[ARG]]) #4
// CHECK-NEXT:    [[INCDEC_PTR:%.*]] = getelementptr inbounds i8, ptr addrspace(200) [[ARG_PTR:%.*]], i64 -1
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef [[ARG_PTR]]) #4
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef [[TMP0]]) #4
// CHECK-NEXT:    call void @check(ptr addrspace(200) noundef nonnull [[INCDEC_PTR]]) #4
// CHECK-NEXT:    ret void
//
void test_cg_postdec(uintptr_t arg, char *arg_ptr) {
  check(arg--);
  check((uintptr_t)arg_ptr--);
  check(arg);
  check((uintptr_t)arg_ptr);
}

// This caused an assertion failure after the initial infer-provenance changes: check that it works
uintptr_t regression(uintptr_t b) {
   return b >> 1;
}
