//===- llvm/MC/MCEmulator.h - Emulator interface ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MC_MCEMULATOR_MCEMULATOR_H
#define LLVM_MC_MCEMULATOR_MCEMULATOR_H

#include "llvm/MC/MCEmulator/MCEmulatorState.h"

namespace llvm {

class MCEmulator
{
public:
    typedef std::unique_ptr<llvm::MCEmulatorState> MCEmulatorStateTy;
    MCEmulator(MCEmulatorStateTy state):
        State(std::move(State)) {}
    virtual void load(const Twine &FileName) = 0;

    void test();
    
private:
    MCEmulator();
    MCEmulatorStateTy State;
};


} // end namespace llvm

#endif // LLVM_MC_MCEMULATOR_MCEMULATOR_H