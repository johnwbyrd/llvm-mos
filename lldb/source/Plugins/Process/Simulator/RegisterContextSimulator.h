//===-- RegisterContextSimulator.h ------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_PROCESS_SIMULATOR_REGISTERCONTEXTSIMULATOR_H
#define LLDB_SOURCE_PLUGINS_PROCESS_SIMULATOR_REGISTERCONTEXTSIMULATOR_H

#include "lldb/Target/RegisterContext.h"
#include "llvm/Emulator/Context.h"
#include "llvm/MC/MCRegisterInfo.h"

#include <string>
#include <vector>

namespace lldb_private {

class RegisterContextSimulator : public RegisterContext {
public:
  RegisterContextSimulator(Thread &thread, uint32_t concrete_frame_idx,
                           llvm::emu::Context *context,
                           const llvm::MCRegisterInfo *mri);
  ~RegisterContextSimulator() override;

  void InvalidateAllRegisters() override;
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
  void BuildRegisterInfo();
  uint32_t GetRegisterSizeInBytes(llvm::MCRegister reg) const;

  llvm::emu::Context *m_context;
  const llvm::MCRegisterInfo *m_mri;

  // Dynamically built register info
  std::vector<RegisterInfo> m_reg_infos;
  std::vector<std::string> m_reg_names;
  std::vector<uint32_t> m_reg_indices;
  RegisterSet m_reg_set;
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_PROCESS_SIMULATOR_REGISTERCONTEXTSIMULATOR_H
