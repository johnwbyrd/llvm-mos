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
  ThreadSimulator(ProcessSimulator &process, lldb::tid_t tid,
                  llvm::emu::Context *context);
  ~ThreadSimulator() override;

  void RefreshStateAfterStop() override;
  lldb::RegisterContextSP GetRegisterContext() override;
  lldb::RegisterContextSP
  CreateRegisterContextForFrame(StackFrame *frame) override;

  llvm::emu::Context *GetEmulatorContext() { return m_context; }

protected:
  bool CalculateStopInfo() override;

private:
  llvm::emu::Context *m_context;
  lldb::RegisterContextSP m_reg_context_sp;
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_PROCESS_SIMULATOR_THREADSIMULATOR_H
