//===-- MOSMachineFuctionInfo.h - MOS machine function info -----*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares MOS-specific per-machine-function information.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MOS_MOSMACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_MOS_MOSMACHINEFUNCTIONINFO_H

#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

class MOSSubtarget;

struct MOSFunctionInfo : public MachineFunctionInfo {
  MOSFunctionInfo(const Function &F, const MOSSubtarget *STI) {}

  MachineFunctionInfo *
  clone(BumpPtrAllocator &Allocator, MachineFunction &DestMF,
        const DenseMap<MachineBasicBlock *, MachineBasicBlock *> &Src2DstMBB)
      const override {
    return DestMF.cloneInfo<MOSFunctionInfo>(*this);
  }

  int VarArgsStackIndex = -1;
  const GlobalValue *StaticStackValue = nullptr;
  const GlobalValue *ZeroPageStackValue = nullptr;
  DenseMap<Register, size_t> CSRZPOffsets;

  /// Frame index for the slot where the return address is saved.
  /// Only valid when MachineFrameInfo::isReturnAddressIsTaken() is true.
  /// -1 means the frame index hasn't been created yet.
  int ReturnAddrFrameIndex = -1;

  int getReturnAddrFrameIndex() const { return ReturnAddrFrameIndex; }
  void setReturnAddrFrameIndex(int FI) { ReturnAddrFrameIndex = FI; }
};

} // namespace llvm

#endif // not LLVM_LIB_TARGET_MOS_MOSMACHINEFUNCTIONINFO_H
