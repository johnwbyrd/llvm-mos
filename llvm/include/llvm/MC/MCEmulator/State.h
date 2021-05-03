//===- llvm/MC/MCEmulatorState.h - Emulator state ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MC_MCEMULATOR_STATE_H
#define LLVM_MC_MCEMULATOR_STATE_H

#include "llvm/MC/MCEmulator/Memory.h"
#include "llvm/MC/MCEmulator/Registers.h"

namespace llvm {
namespace emu {

class Dumpable {
public:
  virtual void dump() const = 0;
};

/// An emulator state typically has registers and memory.  More exotic models
/// may descend from this base class.
class State : public Registers, Memory {};

} // end namespace emu
} // end namespace llvm

#endif // LLVM_MC_MCEMULATOR_STATE_H