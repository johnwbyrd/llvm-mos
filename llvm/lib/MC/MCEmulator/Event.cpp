//===- Event.cpp ----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/MC/MCEmulator/Event.h"

namespace llvm {
namespace emu {

Event::Event() : Executed(false){};

EventResult Event::forward() {
  assert(!Executed && "Event was already executed");
  return doForward();
};

EventResult Event::backward() {
  assert(Executed && "Event has not yet been executed");
  return doBackward();
};

} // namespace emu
} // namespace llvm
