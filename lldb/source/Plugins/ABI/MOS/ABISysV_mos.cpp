//===-- ABISysV_mos.cpp ----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABISysV_mos.h"

#include "lldb/Core/Module.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Core/Value.h"
#include "lldb/Symbol/Symbol.h"
#include "lldb/Symbol/SymbolContext.h"
#include "lldb/Symbol/Symtab.h"
#include "lldb/Symbol/UnwindPlan.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/RegisterContext.h"
#include "lldb/Target/StackFrame.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/RegisterValue.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include "lldb/ValueObject/ValueObjectMemory.h"
#include "lldb/ValueObject/ValueObjectRegister.h"

#include "llvm/IR/DerivedTypes.h"
#include "llvm/TargetParser/Triple.h"

using namespace lldb;
using namespace lldb_private;

LLDB_PLUGIN_DEFINE_ADV(ABISysV_mos, ArchitectureMOS)

// DWARF register numbers for MOS (from MOSRegisterInfo.td)
enum dwarf_regnums {
  dwarf_a = 0,  // Accumulator
  dwarf_x = 2,  // X index register
  dwarf_y = 4,  // Y index register
  dwarf_s = 6,  // Stack pointer
  dwarf_c = 7,  // Carry flag
  dwarf_n = 8,  // Negative flag
  dwarf_v = 9,  // Overflow flag
  dwarf_z = 10, // Zero flag
  dwarf_p = 12, // Processor status
  // Imaginary registers start at 16 (0x10)
  dwarf_imag_8bit_start = 16,
  dwarf_imag_16bit_start = 16 + (256 * 2), // 528
};

static const RegisterInfo g_register_infos[] = {
    {
        "a",
        "acc",
        1,
        0,
        eEncodingUint,
        eFormatHex,
        {dwarf_a, dwarf_a, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,
         LLDB_INVALID_REGNUM},
        nullptr,
        nullptr,
        nullptr,
    },
    {
        "x",
        "",
        1,
        0,
        eEncodingUint,
        eFormatHex,
        {dwarf_x, dwarf_x, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,
         LLDB_INVALID_REGNUM},
        nullptr,
        nullptr,
        nullptr,
    },
    {
        "y",
        "",
        1,
        0,
        eEncodingUint,
        eFormatHex,
        {dwarf_y, dwarf_y, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,
         LLDB_INVALID_REGNUM},
        nullptr,
        nullptr,
        nullptr,
    },
    {
        "s",
        "sp",
        1,
        0,
        eEncodingUint,
        eFormatHex,
        {dwarf_s, dwarf_s, LLDB_REGNUM_GENERIC_SP, LLDB_INVALID_REGNUM,
         LLDB_INVALID_REGNUM},
        nullptr,
        nullptr,
        nullptr,
    },
    {
        "p",
        "status",
        1,
        0,
        eEncodingUint,
        eFormatHex,
        {dwarf_p, dwarf_p, LLDB_REGNUM_GENERIC_FLAGS, LLDB_INVALID_REGNUM,
         LLDB_INVALID_REGNUM},
        nullptr,
        nullptr,
        nullptr,
    },
};

const RegisterInfo *ABISysV_mos::GetRegisterInfoArray(uint32_t &count) {
  count = std::size(g_register_infos);
  return g_register_infos;
}

size_t ABISysV_mos::GetRedZoneSize() const { return 0; }

//------------------------------------------------------------------
// Static Functions
//------------------------------------------------------------------

ABISP ABISysV_mos::CreateInstance(lldb::ProcessSP process_sp,
                                  const ArchSpec &arch) {
  if (arch.GetTriple().getArch() == llvm::Triple::mos) {
    return ABISP(
        new ABISysV_mos(std::move(process_sp), MakeMCRegisterInfo(arch)));
  }
  return ABISP();
}

bool ABISysV_mos::PrepareTrivialCall(Thread &thread, lldb::addr_t sp,
                                     lldb::addr_t pc, lldb::addr_t ra,
                                     llvm::ArrayRef<addr_t> args) const {
  // 6502 doesn't support complex calling conventions in the traditional sense
  // This is a minimal implementation
  return false;
}

bool ABISysV_mos::GetArgumentValues(Thread &thread, ValueList &values) const {
  // 6502 argument passing is very architecture-specific
  // This would need to be implemented based on MOS calling conventions
  return false;
}

Status ABISysV_mos::SetReturnValueObject(lldb::StackFrameSP &frame_sp,
                                         lldb::ValueObjectSP &new_value) {
  return Status::FromErrorString(
      "Setting return values not implemented for MOS");
}

ValueObjectSP ABISysV_mos::GetReturnValueObjectImpl(Thread &thread,
                                                    CompilerType &type) const {
  // Return values typically in accumulator (A register) for 6502
  return ValueObjectSP();
}

UnwindPlanSP ABISysV_mos::CreateFunctionEntryUnwindPlan() {
  // 6502 has no traditional call frames - create a minimal unwind plan
  // that just preserves the current register state
  auto plan_sp = std::make_shared<UnwindPlan>(eRegisterKindDWARF);
  plan_sp->SetSourceName("mos function-entry unwind plan");
  plan_sp->SetSourcedFromCompiler(eLazyBoolNo);
  plan_sp->SetUnwindPlanValidAtAllInstructions(eLazyBoolYes);
  plan_sp->SetUnwindPlanForSignalTrap(eLazyBoolNo);
  plan_sp->SetReturnAddressRegister(LLDB_INVALID_REGNUM);
  
  // Don't add any rows - let LLDB use the current register values as-is
  return plan_sp;
}

UnwindPlanSP ABISysV_mos::CreateDefaultUnwindPlan() {
  // For now, return the same as function entry
  return CreateFunctionEntryUnwindPlan();
}

bool ABISysV_mos::RegisterIsVolatile(const RegisterInfo *reg_info) {
  if (reg_info) {
    const char *name = reg_info->name;
    // A, X, Y are typically volatile in 6502 calling conventions
    if (strcmp(name, "a") == 0 || strcmp(name, "x") == 0 ||
        strcmp(name, "y") == 0 || strcmp(name, "p") == 0) {
      return true;
    }
  }
  return false;
}

void ABISysV_mos::CreateRegisterMapIfNeeded() {
  // Base class handles this
}

ValueObjectSP
ABISysV_mos::GetReturnValueObjectSimple(Thread &thread,
                                        CompilerType &ast_type) const {
  return ValueObjectSP();
}

// MOS-specific imaginary register detection
ABISysV_mos::ImaginaryRegisterConfig ABISysV_mos::DetectImaginaryRegisters() {
  ImaginaryRegisterConfig config;

  ProcessSP process_sp = GetProcessSP();
  if (!process_sp)
    return config;

  TargetSP target_sp = process_sp->CalculateTarget();
  if (!target_sp)
    return config;

  // Scan all loaded modules for imaginary register symbols
  ModuleList &modules = target_sp->GetImages();
  for (size_t i = 0; i < modules.GetSize(); ++i) {
    ModuleSP module_sp = modules.GetModuleAtIndex(i);
    if (!module_sp)
      continue;

    Symtab *symtab = module_sp->GetSymtab();
    if (!symtab)
      continue;

    // Look for __rc0, __rc1, ..., __rcN patterns
    for (uint32_t rc_num = 0; rc_num < 256; ++rc_num) {
      std::string symbol_name = "__rc" + std::to_string(rc_num);
      Symbol *symbol = symtab->FindFirstSymbolWithNameAndType(
          ConstString(symbol_name), eSymbolTypeAbsolute, Symtab::eDebugAny,
          Symtab::eVisibilityAny);
      if (symbol) {
        config.has_imaginary_regs = true;
        config.max_rc_register = std::max(config.max_rc_register, rc_num);
      }
    }

    // RS registers are synthesized from RC register pairs, so we calculate
    // max_rs_register based on available RC registers
    config.max_rs_register = config.max_rc_register / 2;
  }

  return config;
}

void ABISysV_mos::AddImaginaryRegistersToList(
    std::vector<DynamicRegisterInfo::Register> &regs,
    const ImaginaryRegisterConfig &config) {

  ConstString empty_alt_name;
  ConstString reg_set{"imaginary registers"};

  // Add RC registers (8-bit imaginary registers)
  for (uint32_t i = 0; i <= config.max_rc_register; ++i) {
    uint32_t dwarf_num = dwarf_imag_8bit_start + (i * 2);
    std::string name = "rc" + std::to_string(i);

    DynamicRegisterInfo::Register reg{ConstString(name),
                                      empty_alt_name,
                                      reg_set,
                                      1,
                                      LLDB_INVALID_INDEX32,
                                      lldb::eEncodingUint,
                                      lldb::eFormatHex,
                                      LLDB_INVALID_REGNUM,
                                      LLDB_INVALID_REGNUM,
                                      dwarf_num,
                                      LLDB_INVALID_REGNUM,
                                      {},
                                      {}};
    regs.push_back(reg);
  }

  // Add RS registers (16-bit imaginary registers)
  for (uint32_t i = 0; i <= config.max_rs_register; ++i) {
    uint32_t dwarf_num = dwarf_imag_16bit_start + i;
    std::string name = "rs" + std::to_string(i);

    uint32_t generic_reg = LLDB_INVALID_REGNUM;
    // RS15 is the frame pointer
    if (i == 15) {
      generic_reg = LLDB_REGNUM_GENERIC_FP;
    }

    DynamicRegisterInfo::Register reg{ConstString(name),
                                      empty_alt_name,
                                      reg_set,
                                      2,
                                      LLDB_INVALID_INDEX32,
                                      lldb::eEncodingUint,
                                      lldb::eFormatHex,
                                      LLDB_INVALID_REGNUM,
                                      LLDB_INVALID_REGNUM,
                                      dwarf_num,
                                      generic_reg,
                                      {},
                                      {}};
    regs.push_back(reg);
  }
}

void ABISysV_mos::AugmentRegisterInfo(
    std::vector<DynamicRegisterInfo::Register> &regs) {
  // First, call base class to handle standard register info
  RegInfoBasedABI::AugmentRegisterInfo(regs);

  // Detect and add imaginary registers if present
  ImaginaryRegisterConfig config = DetectImaginaryRegisters();
  if (config.has_imaginary_regs) {
    AddImaginaryRegistersToList(regs, config);
  }
}

void ABISysV_mos::Initialize() {
  PluginManager::RegisterPlugin(GetPluginNameStatic(),
                                "System V ABI for MOS targets", CreateInstance);
}

void ABISysV_mos::Terminate() {
  PluginManager::UnregisterPlugin(CreateInstance);
}

