//===-- System.cpp - Multi-CPU System Implementation ------------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Emulator/System.h"
#include "llvm/Emulator/Context.h"

using namespace llvm;
using namespace llvm::emu;

bool System::restoreToCheckpoint(size_t Idx) {
  if (Idx >= Checkpoints.size())
    return false;

  auto &CP = Checkpoints[Idx];

  // Replay undo log backwards to restore previous state
  for (size_t I = UndoLog.size(); I > CP.UndoLogPosition; --I) {
    auto &Rec = UndoLog[I - 1];
    if (Rec.Type == UndoRecord::Memory) {
      // Restore memory without logging (would cause infinite growth)
      routeWriteNoLog(Rec.Key, uint8_t(Rec.OldValue));
    } else {
      // Restore register
      size_t CtxIdx = Rec.Key >> 32;
      unsigned RegNum = Rec.Key & 0xFFFFFFFF;
      if (CtxIdx < Contexts.size()) {
        Contexts[CtxIdx].Ctx->writeRegisterNoLog(RegNum, &Rec.OldValue,
                                                  Rec.Size);
      }
    }
  }

  // Truncate the undo log and checkpoints
  UndoLog.resize(CP.UndoLogPosition);
  Checkpoints.resize(Idx + 1);

  // Clear stop reason
  LastStopReason = StopReason::None;
  LastStopAddress = 0;
  StoppedContext = nullptr;

  return true;
}
