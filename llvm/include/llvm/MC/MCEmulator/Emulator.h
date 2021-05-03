//===- llvm/MC/MCEmulator/Emulator.h - Emulator interface -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MC_MCEMULATOR_EMULATOR_H
#define LLVM_MC_MCEMULATOR_EMULATOR_H

#include <memory>
#include "llvm/MC/MCEmulator/State.h"

namespace llvm {
namespace emu {

class Emulator
{
public:
    typedef std::unique_ptr<State> StateTy;
    Emulator(StateTy state):
        State(std::move(state)) {}
    const State &getState() { return *State; };

protected:
    StateTy State;

private:
    Emulator();
};

} // end namespace emu
} // end namespace llvm

#endif // LLVM_MC_MCEMULATOR_EMULATOR_H