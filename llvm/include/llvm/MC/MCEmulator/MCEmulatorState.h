//===- llvm/MC/MCEmulatorState.h - Emulator state ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MC_MCEMULATOR_MCEMULATORSTATE_H
#define LLVM_MC_MCEMULATOR_MCEMULATORSTATE_H

#include "llvm/Support/Casting.h"
#include "llvm/Support/MemoryBuffer.h"

namespace llvm {

class MCEmulatorState {
public:
  enum EmulatorStateKind { ESK_FlatMemory };
  EmulatorStateKind getKind() const { return Kind; }

private:
  const EmulatorStateKind Kind;

public:
  MCEmulatorState(EmulatorStateKind K) : Kind(K) {}
};

class MCEmulatorStateFlatMemory : public MCEmulatorState {
public:
  MCEmulatorStateFlatMemory(size_t S)
      : MCEmulatorState(ESK_FlatMemory), Size(S) {}
  static bool classof(const MCEmulatorState *S) {
    return S->getKind() == ESK_FlatMemory;
  }

protected:
  std::unique_ptr<WritableMemoryBuffer> Memory;
  size_t Size;
};

} // end namespace llvm

#endif // LLVM_MC_MCEMULATOR_MCEMULATORSTATE_H