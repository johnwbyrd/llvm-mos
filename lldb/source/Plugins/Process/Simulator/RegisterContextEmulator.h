//===-- RegisterContextEmulator.h -------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_PROCESS_SIMULATOR_REGISTERCONTEXTEMULATOR_H
#define LLDB_SOURCE_PLUGINS_PROCESS_SIMULATOR_REGISTERCONTEXTEMULATOR_H

#include "lldb/Target/DynamicRegisterInfo.h"
#include "lldb/Target/RegisterContext.h"
#include "llvm/Emulator/Context.h"

namespace lldb_private {

class ThreadSimulator;

/// RegisterContext that delegates metadata to DynamicRegisterInfo and
/// forwards read/write to an emulator Context.
class RegisterContextEmulator : public RegisterContext {
public:
  RegisterContextEmulator(Thread &thread, uint32_t concrete_frame_idx,
                          DynamicRegisterInfo &reg_info);
  ~RegisterContextEmulator() override;

  void InvalidateAllRegisters() override {}
  size_t GetRegisterCount() override;
  const RegisterInfo *GetRegisterInfoAtIndex(size_t reg) override;
  size_t GetRegisterSetCount() override;
  const RegisterSet *GetRegisterSet(size_t set) override;

  bool ReadRegister(const RegisterInfo *reg_info,
                    RegisterValue &reg_value) override;
  bool WriteRegister(const RegisterInfo *reg_info,
                     const RegisterValue &reg_value) override;

  uint32_t ConvertRegisterKindToRegisterNumber(lldb::RegisterKind kind,
                                               uint32_t num) override;

private:
  /// Get the emulator context from the thread.
  llvm::emu::Context *GetContext();

  DynamicRegisterInfo &m_reg_info;
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_PROCESS_SIMULATOR_REGISTERCONTEXTEMULATOR_H
