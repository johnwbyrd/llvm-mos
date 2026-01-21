//===-- Context.cpp - CPU Execution Context Implementation ------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Emulator/Context.h"
#include "llvm/Emulator/System.h"
#include <cstring>

using namespace llvm;
using namespace llvm::emu;

// Explicit template instantiations for recordAndSet
// These are the common register sizes used by CPU implementations
template void Context::recordAndSet<uint8_t>(unsigned, uint8_t &, uint8_t);
template void Context::recordAndSet<uint16_t>(unsigned, uint16_t &, uint16_t);
template void Context::recordAndSet<uint32_t>(unsigned, uint32_t &, uint32_t);
template void Context::recordAndSet<uint64_t>(unsigned, uint64_t &, uint64_t);
template void Context::recordAndSet<bool>(unsigned, bool &, bool);

template <typename T>
void Context::recordAndSet(unsigned RegNum, T &Reg, T NewVal) {
  if (Sys)
    Sys->recordRegisterWrite(ContextIndex, RegNum, &Reg, sizeof(T));
  Reg = NewVal;
}

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
