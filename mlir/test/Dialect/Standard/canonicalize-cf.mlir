// RUN: mlir-opt %s -allow-unregistered-dialect -pass-pipeline='func(canonicalize)' -split-input-file | FileCheck %s

/// Test the folding of BranchOp.

// CHECK-LABEL: func @br_folding(
func @br_folding() -> i32 {
  // CHECK-NEXT: %[[CST:.*]] = constant 0 : i32
  // CHECK-NEXT: return %[[CST]] : i32
  %c0_i32 = constant 0 : i32
  br ^bb1(%c0_i32 : i32)
^bb1(%x : i32):
  return %x : i32
}

/// Test that pass-through successors of BranchOp get folded.

// CHECK-LABEL: func @br_passthrough(
// CHECK-SAME: %[[ARG0:.*]]: i32, %[[ARG1:.*]]: i32
func @br_passthrough(%arg0 : i32, %arg1 : i32) -> (i32, i32) {
  "foo.switch"() [^bb1, ^bb2, ^bb3] : () -> ()

^bb1:
  // CHECK: ^bb1:
  // CHECK-NEXT: br ^bb3(%[[ARG0]], %[[ARG1]] : i32, i32)

  br ^bb2(%arg0 : i32)

^bb2(%arg2 : i32):
  br ^bb3(%arg2, %arg1 : i32, i32)

^bb3(%arg4 : i32, %arg5 : i32):
  return %arg4, %arg5 : i32, i32
}

/// Test the folding of CondBranchOp with a constant condition.

// CHECK-LABEL: func @cond_br_folding(
func @cond_br_folding(%cond : i1, %a : i32) {
  // CHECK-NEXT: return

  %false_cond = constant 0 : i1
  %true_cond = constant 1 : i1
  cond_br %cond, ^bb1, ^bb2(%a : i32)

^bb1:
  cond_br %true_cond, ^bb3, ^bb2(%a : i32)

^bb2(%x : i32):
  cond_br %false_cond, ^bb2(%x : i32), ^bb3

^bb3:
  return
}

/// Test the folding of CondBranchOp when the successors are identical.

// CHECK-LABEL: func @cond_br_same_successor(
func @cond_br_same_successor(%cond : i1, %a : i32) {
  // CHECK-NEXT: return

  cond_br %cond, ^bb1(%a : i32), ^bb1(%a : i32)

^bb1(%result : i32):
  return
}

/// Test the folding of CondBranchOp when the successors are identical, but the
/// arguments are different.

// CHECK-LABEL: func @cond_br_same_successor_insert_select(
// CHECK-SAME: %[[COND:.*]]: i1, %[[ARG0:.*]]: i32, %[[ARG1:.*]]: i32
func @cond_br_same_successor_insert_select(%cond : i1, %a : i32, %b : i32) -> i32 {
  // CHECK: %[[RES:.*]] = select %[[COND]], %[[ARG0]], %[[ARG1]]
  // CHECK: return %[[RES]]

  cond_br %cond, ^bb1(%a : i32), ^bb1(%b : i32)

^bb1(%result : i32):
  return %result : i32
}

/// Check that we don't generate a select if the type requires a splat.
/// TODO: SelectOp should allow for matching a vector/tensor with i1.

// CHECK-LABEL: func @cond_br_same_successor_no_select_tensor(
func @cond_br_same_successor_no_select_tensor(%cond : i1, %a : tensor<2xi32>,
                                              %b : tensor<2xi32>) -> tensor<2xi32>{
  // CHECK: cond_br

  cond_br %cond, ^bb1(%a : tensor<2xi32>), ^bb1(%b : tensor<2xi32>)

^bb1(%result : tensor<2xi32>):
  return %result : tensor<2xi32>
}

// CHECK-LABEL: func @cond_br_same_successor_no_select_vector(
func @cond_br_same_successor_no_select_vector(%cond : i1, %a : vector<2xi32>,
                                              %b : vector<2xi32>) -> vector<2xi32> {
  // CHECK: cond_br

  cond_br %cond, ^bb1(%a : vector<2xi32>), ^bb1(%b : vector<2xi32>)

^bb1(%result : vector<2xi32>):
  return %result : vector<2xi32>
}

/// Test the compound folding of BranchOp and CondBranchOp.

// CHECK-LABEL: func @cond_br_and_br_folding(
func @cond_br_and_br_folding(%a : i32) {
  // CHECK-NEXT: return

  %false_cond = constant 0 : i1
  %true_cond = constant 1 : i1
  cond_br %true_cond, ^bb2, ^bb1(%a : i32)

^bb1(%x : i32):
  cond_br %false_cond, ^bb1(%x : i32), ^bb2

^bb2:
  return
}

/// Test that pass-through successors of CondBranchOp get folded.

// CHECK-LABEL: func @cond_br_passthrough(
// CHECK-SAME: %[[ARG0:.*]]: i32, %[[ARG1:.*]]: i32, %[[ARG2:.*]]: i32, %[[COND:.*]]: i1
func @cond_br_passthrough(%arg0 : i32, %arg1 : i32, %arg2 : i32, %cond : i1) -> (i32, i32) {
  // CHECK: %[[RES:.*]] = select %[[COND]], %[[ARG0]], %[[ARG2]]
  // CHECK: %[[RES2:.*]] = select %[[COND]], %[[ARG1]], %[[ARG2]]
  // CHECK: return %[[RES]], %[[RES2]]

  cond_br %cond, ^bb1(%arg0 : i32), ^bb2(%arg2, %arg2 : i32, i32)

^bb1(%arg3: i32):
  br ^bb2(%arg3, %arg1 : i32, i32)

^bb2(%arg4: i32, %arg5: i32):
  return %arg4, %arg5 : i32, i32
}

/// Test the failure modes of collapsing CondBranchOp pass-throughs successors.

// CHECK-LABEL: func @cond_br_pass_through_fail(
func @cond_br_pass_through_fail(%cond : i1) {
  // CHECK: cond_br %{{.*}}, ^bb1, ^bb2

  cond_br %cond, ^bb1, ^bb2

^bb1:
  // CHECK: ^bb1:
  // CHECK: "foo.op"
  // CHECK: br ^bb2

  // Successors can't be collapsed if they contain other operations.
  "foo.op"() : () -> ()
  br ^bb2

^bb2:
  return
}
