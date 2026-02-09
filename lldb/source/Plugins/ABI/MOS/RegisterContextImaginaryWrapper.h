//===-- RegisterContextImaginaryWrapper.h -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// A decorator that wraps any RegisterContext and adds MOS imaginary register
// support. Imaginary registers (RC0-RCn, RS0-RSn) are stored in zero-page
// memory rather than CPU registers. This wrapper intercepts reads/writes for
// imaginary registers and routes them through Process memory access.
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_ABI_MOS_REGISTERCONTEXTIMAGINARYWRAPPER_H
#define LLDB_SOURCE_PLUGINS_ABI_MOS_REGISTERCONTEXTIMAGINARYWRAPPER_H

#include "MOSImaginaryRegisters.h"
#include "lldb/Target/RegisterContext.h"

namespace lldb_private {

/// A decorator that adds MOS imaginary register support to any RegisterContext.
///
/// This wrapper can be applied to any RegisterContext (GDBRemote, Emulator,
/// etc.) to add transparent support for MOS imaginary registers. When a
/// read/write targets an imaginary register, it's routed through memory
/// access instead of the underlying register context.
class RegisterContextImaginaryWrapper : public RegisterContext {
public:
  /// Create a wrapper around an existing RegisterContext.
  /// @param thread The thread owning this register context.
  /// @param frame_idx The concrete frame index.
  /// @param base The underlying RegisterContext to wrap.
  /// @param imag_regs The imaginary register helper for address lookups.
  RegisterContextImaginaryWrapper(Thread &thread, uint32_t frame_idx,
                                  lldb::RegisterContextSP base,
                                  const MOSImaginaryRegisters &imag_regs);

  ~RegisterContextImaginaryWrapper() override;

  // Delegate all metadata methods to the base context
  void InvalidateAllRegisters() override { m_base->InvalidateAllRegisters(); }
  size_t GetRegisterCount() override { return m_base->GetRegisterCount(); }
  const RegisterInfo *GetRegisterInfoAtIndex(size_t reg) override {
    return m_base->GetRegisterInfoAtIndex(reg);
  }
  size_t GetRegisterSetCount() override { return m_base->GetRegisterSetCount(); }
  const RegisterSet *GetRegisterSet(size_t set) override {
    return m_base->GetRegisterSet(set);
  }
  uint32_t ConvertRegisterKindToRegisterNumber(lldb::RegisterKind kind,
                                               uint32_t num) override {
    return m_base->ConvertRegisterKindToRegisterNumber(kind, num);
  }

  // Override read/write to handle imaginary registers
  bool ReadRegister(const RegisterInfo *reg_info,
                    RegisterValue &reg_value) override;
  bool WriteRegister(const RegisterInfo *reg_info,
                     const RegisterValue &reg_value) override;

private:
  bool ReadImaginaryRegister(uint32_t dwarf_num, uint32_t byte_size,
                             RegisterValue &value);
  bool WriteImaginaryRegister(uint32_t dwarf_num, uint32_t byte_size,
                              const RegisterValue &value);

  lldb::RegisterContextSP m_base;
  const MOSImaginaryRegisters &m_imag_regs;
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_ABI_MOS_REGISTERCONTEXTIMAGINARYWRAPPER_H
