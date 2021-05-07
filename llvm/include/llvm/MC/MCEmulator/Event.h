//===- llvm/MC/MCEmulator/Event.h - Emulator events -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MC_MCEMULATOR_EVENT_H
#define LLVM_MC_MCEMULATOR_EVENT_H

#include "llvm/ADT/SmallVector.h"
#include <cassert>

namespace llvm {
namespace emu {

enum EventResult {
  OK,            //< step completed successfully
  NoMoreUndo,    //< undo stack is empty so no undo is possible
  NotImplemented //< not yet implemented
};

class Event {
public:
  Event();
  virtual EventResult forward();
  virtual EventResult backward();

protected:
  virtual EventResult doForward() = 0;
  virtual EventResult doBackward() = 0;

  bool Executed;
};

class MultiEvent : public Event {
public:

protected:
    typedef SmallVector<std::unique_ptr<Event>, 2> EventsTy;
    EventsTy Events;
};


} // end namespace emu
} // end namespace llvm

#endif // LLVM_MC_MCEMULATOR_EVENT_H