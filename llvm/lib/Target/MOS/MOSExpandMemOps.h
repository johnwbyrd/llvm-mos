//===-- MOSExpandMemOps.h - MOS Memory Operation Expansion ------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the MOS memory operation expansion pass.
//
// This pass expands MemCpy, MemMove, and MemSet pseudo-instructions into
// efficient Y-indexed loops after register allocation.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MOS_MOSEXPANDMEMOPS_H
#define LLVM_LIB_TARGET_MOS_MOSEXPANDMEMOPS_H

#include "llvm/CodeGen/MachineFunctionPass.h"

namespace llvm {

MachineFunctionPass *createMOSExpandMemOpsPass();

} // namespace llvm

#endif // LLVM_LIB_TARGET_MOS_MOSEXPANDMEMOPS_H
