// NOTE: Assertions have been autogenerated by utils/update_cc_test_checks.py UTC_ARGS: --function-signature
// RUN: %cheri_cc1 -o - -emit-llvm -O1 %s -verify | FileCheck %s
// https://github.com/CTSRD-CHERI/llvm-project/issues/338

struct a {
  void (*__capability cap_fnptr)();
};

// CHECK-LABEL: define {{[^@]+}}@test1
// CHECK-SAME: (ptr addrspace(200) inreg readnone [[ARG_COERCE:%.*]]) local_unnamed_addr #[[ATTR0:[0-9]+]] {
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[TMP0:%.*]] = tail call ptr addrspace(200) @llvm.cheri.ddc.get()
// CHECK-NEXT:    [[TMP1:%.*]] = tail call i64 @llvm.cheri.cap.to.pointer.i64(ptr addrspace(200) [[TMP0]], ptr addrspace(200) [[ARG_COERCE]])
// CHECK-NEXT:    ret i64 [[TMP1]]
//
long test1(struct a arg) {
  return (long)arg.cap_fnptr; // expected-warning{{the following conversion will result in a CToPtr operation; the behaviour of CToPtr can  be confusing since using CToPtr on an untagged capability will give 0 instead of the  integer value and should therefore be explicitly annotated}}
  //expected-note@-1{{if you really intended to use CToPtr use __builtin_cheri_cap_to_pointer() or a __cheri_fromcap cast to silence this warning; to get the virtual address use __builtin_cheri_address_get() or a __cheri_addr cast; to get the capability offset use __builtin_cheri_offset_get() or a __cheri_offset cast}}
}

// CHECK-LABEL: define {{[^@]+}}@test2
// CHECK-SAME: (ptr addrspace(200) inreg readnone [[ARG_COERCE:%.*]]) local_unnamed_addr #[[ATTR0]] {
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[TMP0:%.*]] = tail call i64 @llvm.cheri.cap.offset.get.i64(ptr addrspace(200) [[ARG_COERCE]])
// CHECK-NEXT:    ret i64 [[TMP0]]
//
long test2(struct a arg) {
  return (__cheri_offset long)arg.cap_fnptr;
}

// CHECK-LABEL: define {{[^@]+}}@test3
// CHECK-SAME: (ptr addrspace(200) inreg readnone [[ARG_COERCE:%.*]]) local_unnamed_addr #[[ATTR0]] {
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[TMP0:%.*]] = tail call i64 @llvm.cheri.cap.address.get.i64(ptr addrspace(200) [[ARG_COERCE]])
// CHECK-NEXT:    ret i64 [[TMP0]]
//
long test3(struct a arg) {
  return (__cheri_addr long)arg.cap_fnptr;
}

// CHECK-LABEL: define {{[^@]+}}@test4
// CHECK-SAME: (ptr addrspace(200) inreg [[ARG_COERCE:%.*]]) local_unnamed_addr #[[ATTR0]] {
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[TMP0:%.*]] = tail call i64 @llvm.cheri.cap.to.pointer.i64(ptr addrspace(200) [[ARG_COERCE]], ptr addrspace(200) [[ARG_COERCE]])
// CHECK-NEXT:    [[TMP1:%.*]] = inttoptr i64 [[TMP0]] to ptr
// CHECK-NEXT:    ret ptr [[TMP1]]
//
void* test4(struct a arg) {
  return __builtin_cheri_cap_to_pointer(arg.cap_fnptr, arg.cap_fnptr);
}
