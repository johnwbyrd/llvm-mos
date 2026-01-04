// RUN: %clang_cc1 -triple mos -emit-llvm -o - %s | FileCheck %s

// Test that __builtin_frame_address and __builtin_return_address compile
// correctly on MOS (16-bit int target). The intrinsics require i32 depth
// argument, so clang must zero-extend the 16-bit UnsignedIntTy.

void* test_frame_address(void) {
    // CHECK: call ptr @llvm.frameaddress.p0(i32 0)
    return __builtin_frame_address(0);
}

void* test_return_address(void) {
    // CHECK: call ptr @llvm.returnaddress(i32 0)
    return __builtin_return_address(0);
}
