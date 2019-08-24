//===-- MOSFrameLowering.cpp - MOS Frame Information ----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the MOS implementation of TargetFrameLowering class.
//
//===----------------------------------------------------------------------===//

#include "MOSFrameLowering.h"

#include "MOS.h"
#include "MOSInstrInfo.h"
#include "MOSMachineFunctionInfo.h"
#include "MOSTargetMachine.h"
#include "MCTargetDesc/MOSMCTargetDesc.h"

#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/Function.h"

#include <vector>

namespace llvm {

MOSFrameLowering::MOSFrameLowering()
    : TargetFrameLowering(TargetFrameLowering::StackGrowsDown, 1, -2) {}
}
