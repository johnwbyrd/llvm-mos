//===-- Context.cpp - CPU Execution Context Implementation ------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Emulator/Context.h"
#include "llvm/Emulator/System.h"

using namespace llvm;
using namespace llvm::emu;

bool Context::run() {
  while (!isHalted()) {
    if (!step())
      return false;
  }
  return true;
}

uint64_t Context::executeFor(uint64_t TargetCycles) {
  uint64_t StartCycles = getCycles();
  uint64_t EndCycles = StartCycles + TargetCycles;

  while (!isHalted() && getCycles() < EndCycles) {
    if (!step())
      break;
  }

  return getCycles() - StartCycles;
}

uint8_t Context::read(uint64_t Addr) {
  if (Sys)
    return Sys->read(Addr);
  return 0xFF;
}

void Context::write(uint64_t Addr, uint8_t Value) {
  if (Sys)
    Sys->write(Addr, Value);
}
