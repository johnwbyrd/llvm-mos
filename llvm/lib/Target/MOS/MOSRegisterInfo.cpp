//===-- MOSRegisterInfo.cpp - MOS Register Information --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the MOS implementation of the TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#include "MOSRegisterInfo.h"

#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/CodeGen/TargetFrameLowering.h"

#include "MOS.h"
#include "MOSInstrInfo.h"
#include "MOSTargetMachine.h"
#include "MCTargetDesc/MOSMCTargetDesc.h"

#define GET_REGINFO_TARGET_DESC
#include "MOSGenRegisterInfo.inc"

namespace llvm {

MOSRegisterInfo::MOSRegisterInfo() : MOSGenRegisterInfo(0) {}

} // end of namespace llvm
