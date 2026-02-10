//===-- ThreadSimulator.h ---------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_PROCESS_SIMULATOR_THREADSIMULATOR_H
#define LLDB_SOURCE_PLUGINS_PROCESS_SIMULATOR_THREADSIMULATOR_H

#include "lldb/Target/Thread.h"
#include "llvm/Emulator/Context.h"

namespace lldb_private {

class ProcessSimulator;

class ThreadSimulator : public Thread {
public:
  /// Create a thread for the given emulator context.
  /// @param process The parent process.
  /// @param tid Thread ID (1-based, for LLDB).
  /// @param context_idx Index of the emulator context in System (0-based).
  ThreadSimulator(ProcessSimulator &process, lldb::tid_t tid,
                  size_t context_idx);
  ~ThreadSimulator() override;

  void RefreshStateAfterStop() override;
  lldb::RegisterContextSP GetRegisterContext() override;
  lldb::RegisterContextSP
  CreateRegisterContextForFrame(StackFrame *frame) override;

  /// Get the emulator context for this thread.
  llvm::emu::Context *GetEmulatorContext();

protected:
  bool CalculateStopInfo() override;

private:
  size_t m_context_idx;
  lldb::RegisterContextSP m_reg_context_sp;
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_PROCESS_SIMULATOR_THREADSIMULATOR_H
