// RUN: mlir-opt \
// RUN: -load-pass-plugin=%mlir_lib_dir/leonova_a_4_MLIR%shlibext \
// RUN: --pass-pipeline="builtin.module(memref-copy-to-loop)" \
// RUN: %s | FileCheck %s

func.func @copy_basic(%src : memref<8xf32>,
                      %dst : memref<8xf32>) {
  memref.copy %src, %dst : memref<8xf32> to memref<8xf32>
  return
}

// CHECK-LABEL: func.func @copy_basic
// CHECK: scf.for
// CHECK: memref.load
// CHECK: memref.store
// CHECK-NOT: memref.copy

func.func @copy_iv(%a: memref<4xi32>,
                   %b: memref<4xi32>) {
  memref.copy %a, %b : memref<4xi32> to memref<4xi32>
  return
}

// CHECK-LABEL: func.func @copy_iv
// CHECK: scf.for %{{.*}} = %{{.*}} to %{{.*}} step %{{.*}}
// CHECK: memref.load
// CHECK: memref.store
// CHECK-NOT: memref.copy

func.func @multi(%a: memref<4xi32>,
                 %b: memref<4xi32>,
                 %c: memref<4xi32>) {
  memref.copy %a, %b : memref<4xi32> to memref<4xi32>
  memref.copy %b, %c : memref<4xi32> to memref<4xi32>
  return
}

// CHECK-LABEL: func.func @multi
// CHECK: scf.for
// CHECK: scf.for
// CHECK: memref.load
// CHECK: memref.store
// CHECK-NOT: memref.copy

func.func @no_copy(%a: memref<4xi32>) {
  %i = arith.constant 0 : index
  %v = memref.load %a[%i] : memref<4xi32>
  return
}

// CHECK-LABEL: func.func @no_copy
// CHECK-NOT: scf.for
// CHECK-NOT: memref.copy

func.func @stress(%a: memref<32xi32>,
                  %b: memref<32xi32>) {
  memref.copy %a, %b : memref<32xi32> to memref<32xi32>
  return
}

// CHECK-LABEL: func.func @stress
// CHECK: scf.for
// CHECK: memref.load
// CHECK: memref.store
// CHECK-NOT: memref.copy