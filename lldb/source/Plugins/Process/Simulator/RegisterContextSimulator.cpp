//===-- RegisterContextSimulator.cpp --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "RegisterContextSimulator.h"
#include "lldb/Utility/RegisterValue.h"

using namespace lldb;
using namespace lldb_private;

RegisterContextSimulator::RegisterContextSimulator(
    Thread &thread, uint32_t frame_idx, llvm::emu::Context *context,
    const llvm::MCRegisterInfo *mri)
    : RegisterContext(thread, frame_idx), m_context(context), m_mri(mri) {
  BuildRegisterInfo();
}

RegisterContextSimulator::~RegisterContextSimulator() = default;

uint32_t
RegisterContextSimulator::GetRegisterSizeInBytes(llvm::MCRegister reg) const {
  // Find the smallest register class containing this register
  for (const auto &rc : m_mri->regclasses()) {
    if (rc.contains(reg)) {
      unsigned bits = rc.getSizeInBits();
      if (bits > 0)
        return (bits + 7) / 8;
    }
  }
  // Default to 1 byte if we can't determine size
  return 1;
}

void RegisterContextSimulator::BuildRegisterInfo() {
  unsigned num_regs = m_context->getNumRegisters();
  m_reg_infos.reserve(num_regs);
  m_reg_names.reserve(num_regs);

  llvm::MCRegister pc_reg = m_mri->getProgramCounter();

  for (unsigned dwarf_num = 0; dwarf_num < num_regs; ++dwarf_num) {
    // Map DWARF number to MC register
    auto mc_reg_opt = m_mri->getLLVMRegNum(dwarf_num, false);
    if (!mc_reg_opt)
      continue;

    llvm::MCRegister mc_reg = *mc_reg_opt;
    const char *name = m_mri->getName(mc_reg);
    if (!name)
      continue;

    // Store name (RegisterInfo needs stable pointers)
    m_reg_names.push_back(name);

    uint32_t byte_size = GetRegisterSizeInBytes(mc_reg);

    // Determine generic register kind
    uint32_t generic_kind = LLDB_INVALID_REGNUM;
    if (mc_reg == pc_reg || llvm::StringRef(name).equals_insensitive("pc")) {
      generic_kind = LLDB_REGNUM_GENERIC_PC;
    } else if (llvm::StringRef(name).equals_insensitive("sp") ||
               llvm::StringRef(name).equals_insensitive("s")) {
      generic_kind = LLDB_REGNUM_GENERIC_SP;
    }

    RegisterInfo info = {};
    info.name = m_reg_names.back().c_str();
    info.alt_name = nullptr;
    info.byte_size = byte_size;
    info.byte_offset = dwarf_num; // Use DWARF num as offset for simplicity
    info.encoding = eEncodingUint;
    info.format = eFormatHex;
    info.kinds[eRegisterKindEHFrame] = LLDB_INVALID_REGNUM;
    info.kinds[eRegisterKindDWARF] = dwarf_num;
    info.kinds[eRegisterKindGeneric] = generic_kind;
    info.kinds[eRegisterKindProcessPlugin] = dwarf_num;
    info.kinds[eRegisterKindLLDB] = m_reg_infos.size();
    info.value_regs = nullptr;
    info.invalidate_regs = nullptr;
    info.flags_type = nullptr;

    m_reg_infos.push_back(info);
  }

  // Build register indices array (required by LLDB)
  m_reg_indices.resize(m_reg_infos.size());
  for (size_t i = 0; i < m_reg_infos.size(); ++i)
    m_reg_indices[i] = i;

  // Build register set
  m_reg_set.name = "General Purpose Registers";
  m_reg_set.short_name = "gpr";
  m_reg_set.num_registers = m_reg_infos.size();
  m_reg_set.registers = m_reg_indices.data();
}

void RegisterContextSimulator::InvalidateAllRegisters() {
  // No caching - reads go directly to emulator
}

size_t RegisterContextSimulator::GetRegisterCount() {
  return m_reg_infos.size();
}

const RegisterInfo *
RegisterContextSimulator::GetRegisterInfoAtIndex(size_t reg) {
  if (reg < m_reg_infos.size())
    return &m_reg_infos[reg];
  return nullptr;
}

size_t RegisterContextSimulator::GetRegisterSetCount() { return 1; }

const RegisterSet *RegisterContextSimulator::GetRegisterSet(size_t set) {
  if (set == 0)
    return &m_reg_set;
  return nullptr;
}

bool RegisterContextSimulator::ReadRegister(const RegisterInfo *reg_info,
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

bool RegisterContextSimulator::WriteRegister(const RegisterInfo *reg_info,
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

uint32_t RegisterContextSimulator::ConvertRegisterKindToRegisterNumber(
    RegisterKind kind, uint32_t num) {
  for (size_t i = 0; i < m_reg_infos.size(); ++i) {
    if (m_reg_infos[i].kinds[kind] == num)
      return i;
  }
  return LLDB_INVALID_REGNUM;
}
