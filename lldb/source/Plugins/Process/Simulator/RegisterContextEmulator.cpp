//===-- RegisterContextEmulator.cpp ---------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "RegisterContextEmulator.h"
#include "lldb/Utility/RegisterValue.h"

using namespace lldb;
using namespace lldb_private;

RegisterContextEmulator::RegisterContextEmulator(Thread &thread,
                                                 uint32_t frame_idx,
                                                 DynamicRegisterInfo &reg_info,
                                                 llvm::emu::Context *context)
    : RegisterContext(thread, frame_idx), m_reg_info(reg_info),
      m_context(context) {}

RegisterContextEmulator::~RegisterContextEmulator() = default;

size_t RegisterContextEmulator::GetRegisterCount() {
  return m_reg_info.GetNumRegisters();
}

const RegisterInfo *RegisterContextEmulator::GetRegisterInfoAtIndex(size_t reg) {
  return m_reg_info.GetRegisterInfoAtIndex(reg);
}

size_t RegisterContextEmulator::GetRegisterSetCount() {
  return m_reg_info.GetNumRegisterSets();
}

const RegisterSet *RegisterContextEmulator::GetRegisterSet(size_t set) {
  return m_reg_info.GetRegisterSet(set);
}

bool RegisterContextEmulator::ReadRegister(const RegisterInfo *reg_info,
                                           RegisterValue &reg_value) {
  if (!reg_info || !m_context)
    return false;

  unsigned dwarf_num = reg_info->kinds[eRegisterKindDWARF];
  size_t size = reg_info->byte_size;

  uint8_t buf[8] = {0};
  if (!m_context->readRegister(dwarf_num, buf, size))
    return false;

  reg_value.SetBytes(buf, size, eByteOrderLittle);
  return true;
}

bool RegisterContextEmulator::WriteRegister(const RegisterInfo *reg_info,
                                            const RegisterValue &reg_value) {
  if (!reg_info || !m_context)
    return false;

  unsigned dwarf_num = reg_info->kinds[eRegisterKindDWARF];
  size_t size = reg_info->byte_size;

  uint8_t buf[8] = {0};
  if (size <= sizeof(buf)) {
    memcpy(buf, reg_value.GetBytes(), size);
    return m_context->writeRegister(dwarf_num, buf, size);
  }
  return false;
}

uint32_t RegisterContextEmulator::ConvertRegisterKindToRegisterNumber(
    RegisterKind kind, uint32_t num) {
  return m_reg_info.ConvertRegisterKindToRegisterNumber(kind, num);
}
