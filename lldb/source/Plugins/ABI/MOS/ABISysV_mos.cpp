//===-- ABISysV_mos.cpp ----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABISysV_mos.h"
#include "LLDBMOSLog.h"

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

// Placeholder for MOSGDBRemoteRegisterContext
#include "MOSGDBRemoteRegisterContext.h"

// Include for ThreadGDBRemote
#include "Plugins/Process/gdb-remote/ThreadGDBRemote.h"
#include "lldb/lldb-defines.h"
#include "lldb/lldb-enumerations.h"

using namespace lldb;
using namespace lldb_private;

LLDB_PLUGIN_DEFINE_ADV(ABISysV_mos, ArchitectureMOS)

// DWARF register numbers for MOS (from MOSRegisterInfo.td)
enum dwarf_regnums {
  dwarf_a = 0,   // Accumulator
  dwarf_x = 2,   // X index register
  dwarf_y = 4,   // Y index register
  dwarf_s = 6,   // Stack pointer
  dwarf_c = 7,   // Carry flag
  dwarf_n = 8,   // Negative flag
  dwarf_v = 9,   // Overflow flag
  dwarf_z = 10,  // Zero flag
  dwarf_p = 12,  // Processor status (SR)
  dwarf_pc = 14, // Processor status (SR)

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
        /* As a reminder about the meanings of the following elements:
         * eRegisterKindEHFrame = 0, ///< the register numbers seen in eh_frame
         * eRegisterKindDWARF,       ///< the register numbers seen DWARF
         * eRegisterKindGeneric,     ///< insn ptr reg, stack ptr reg, etc not
                                     ///< specific to
                                     ///< any particular target
         * eRegisterKindProcessPlugin, ///< num used by the process plugin -
                                       ///< e.g. by the remote gdb-protocol
                                       ///< stub program
         * eRegisterKindLLDB,         ///< lldb's internal register numbers
         * kNumRegisterKinds
         */
        {dwarf_a, dwarf_a, LLDB_INVALID_REGNUM, 0, 0},
        nullptr,
        nullptr,
        nullptr,
    },
    {
        "x",
        "",
        1,
        1,
        eEncodingUint,
        eFormatHex,
        {dwarf_x, dwarf_x, LLDB_INVALID_REGNUM, 1, 1},
        nullptr,
        nullptr,
        nullptr,
    },
    {
        "y",
        "",
        1,
        2,
        eEncodingUint,
        eFormatHex,
        {dwarf_y, dwarf_y, LLDB_INVALID_REGNUM, 2, 2},
        nullptr,
        nullptr,
        nullptr,
    },
    {
        "p",
        "sr",
        1,
        3,
        eEncodingUint,
        eFormatHex,
        {dwarf_p, dwarf_p, LLDB_REGNUM_GENERIC_FLAGS, 3, 3},
        nullptr,
        nullptr,
        nullptr,
    },
    /* In MAME, the 6502 stack register is considered to be a two-byte register,
     * even though it is only 1 byte in size.  We'll roll with that for now */
    /* The status register */
    {
        "sp",
        "s",
        2,
        4,
        eEncodingUint,
        eFormatHex,
        {dwarf_s, dwarf_s, LLDB_REGNUM_GENERIC_SP, 4, 4},
        nullptr,
        nullptr,
        nullptr,
    },
    /* In MAME, the 6502 stack register is considered to be a two-byte register,
     * even though it is only 1 byte in size.  We'll roll with that for now */
    /* The status register */
    {
        "pc",
        "",
        2,
        6,
        eEncodingUint,
        eFormatHex,
        {dwarf_pc, dwarf_pc, LLDB_REGNUM_GENERIC_PC, 5, 5},
        nullptr,
        nullptr,
        nullptr,
    }};

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
  auto plan_sp = std::make_shared<UnwindPlan>(eRegisterKindGeneric);
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
  // Nothing ever happens behind your back on MOS, so no volatile registers
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

// Helper to get the address of an imaginary register symbol.
// NOTE: This is the ONLY valid way to get an imaginary register's address.
// Always use GetRawValue() for these symbols, as GetLoadAddress() and others
// may be incorrect for absolute symbols.
static lldb::addr_t GetImaginaryRegisterAddress(Symbol *symbol) {
  return symbol ? symbol->GetRawValue() : LLDB_INVALID_ADDRESS;
}

// Define the static member
std::unordered_map<std::string, lldb::addr_t>
    ABISysV_mos::imaginary_register_map;

ABISysV_mos::ImaginaryRegisterConfig ABISysV_mos::DetectImaginaryRegisters() {
  ImaginaryRegisterConfig config;

  // Only populate if the map is empty
  if (!imaginary_register_map.empty()) {
    config.has_imaginary_regs = true;
    // Compute max_rc_register from the map
    for (const auto &pair : imaginary_register_map) {
      if (pair.first.size() > 2 && pair.first.substr(0, 2) == "rc") {
        int rc_num = std::atoi(pair.first.c_str() + 2);
        config.max_rc_register =
            std::max(config.max_rc_register, static_cast<uint32_t>(rc_num));
      }
    }
    config.max_rs_register = config.max_rc_register / 2;
    return config;
  }

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
        config.max_rc_register =
            std::max(config.max_rc_register, static_cast<uint32_t>(rc_num));
        std::string map_key = "rc" + std::to_string(rc_num);
        lldb::addr_t addr = GetImaginaryRegisterAddress(symbol);
        LLDB_MOS_LOG_REG("Populating imaginary_register_map_: key='{}', "
                         "value=0x{:x} (from symbol '{}')",
                         map_key, addr, symbol_name);
        imaginary_register_map[map_key] = addr;
      }
    }

    // RS registers are synthesized from RC register pairs, so we calculate
    // max_rs_register based on available RC registers
    config.max_rs_register = config.max_rc_register / 2;
  }

  // Log the map after population
  // LogImaginaryRegisterMap();
  // LLDB_MOS_LOG_REG("[DetectImaginaryRegisters] ABI this={0}, map addr={1},
  // map size={2}", (void*)this, (void*)&imaginary_register_map_,
  // imaginary_register_map_.size());
  return config;
}

void ABISysV_mos::AddImaginaryRegistersToList(
    std::vector<DynamicRegisterInfo::Register> &regs,
    const ImaginaryRegisterConfig &config) {

  ConstString empty_alt_name;
  ConstString reg_set{"imaginary"};

  // Find the next available offset and regnum
  uint32_t next_offset = 0;
  uint32_t next_regnum = 0;
  for (const auto &reg : regs) {
    if (reg.byte_offset != LLDB_INVALID_INDEX32)
      next_offset = std::max(next_offset, reg.byte_offset + reg.byte_size);
    if (reg.regnum_remote != LLDB_INVALID_REGNUM)
      next_regnum = std::max(next_regnum, reg.regnum_remote + 1);
  }

  // Add RC registers (8-bit imaginary registers)
  for (uint32_t i = 0; i <= config.max_rc_register; ++i) {
    uint32_t dwarf_num = dwarf_imag_8bit_start + (i * 2);
    std::string name = "rc" + std::to_string(i);

    DynamicRegisterInfo::Register reg{ConstString(name),
                                      empty_alt_name,
                                      reg_set,
                                      1,
                                      next_offset,
                                      lldb::eEncodingUint,
                                      lldb::eFormatHex,
                                      dwarf_num,
                                      dwarf_num,
                                      LLDB_INVALID_REGNUM,
                                      LLDB_INVALID_REGNUM,
                                      {},
                                      {}};
    reg.regnum_remote = next_regnum++;
    next_offset += 1;
    regs.push_back(reg);
  }

  // Add RS registers (16-bit imaginary registers)
  for (uint32_t i = 0; i <= config.max_rs_register; ++i) {
    uint32_t dwarf_num = dwarf_imag_16bit_start + i;
    std::string name = "rs" + std::to_string(i);

    uint32_t generic_reg = LLDB_INVALID_REGNUM;
    // Should we get this information out of DWARF, instead of assuming?
    if (i == 0) {
      generic_reg = LLDB_REGNUM_GENERIC_FP;
    }

    DynamicRegisterInfo::Register reg{ConstString(name),
                                      empty_alt_name,
                                      reg_set,
                                      2,
                                      next_offset,
                                      lldb::eEncodingUint,
                                      lldb::eFormatHex,
                                      dwarf_num,
                                      dwarf_num,
                                      generic_reg,
                                      LLDB_INVALID_REGNUM,
                                      {},
                                      {}};
    reg.regnum_remote = next_regnum++;
    next_offset += 2;
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

  // --- BEGIN LOGGING PATCH ---
  LLDB_MOS_LOG_REG(
      "[AugmentRegisterInfo] Dumping register info after augmentation:");
  int idx = 0;
  for (const auto &reg : regs) {
    LLDB_MOS_LOG_REG(
        "  [{0:2}] name='{1}', size={2}, offset={3}, encoding={4}, format={5}, "
        "generic={6}, dwarf={7}, ehframe={8}, set='{9}'",
        idx++, reg.name ? reg.name.AsCString() : "<null>", reg.byte_size,
        reg.byte_offset, reg.encoding, reg.format, reg.regnum_generic,
        reg.regnum_dwarf, reg.regnum_ehframe,
        reg.set_name ? reg.set_name.AsCString() : "<null>");
  }
  // --- END LOGGING PATCH ---
}

void ABISysV_mos::Initialize() {
  RegisterLLDBMOSLogChannel();
  PluginManager::RegisterPlugin(GetPluginNameStatic(),
                                "System V ABI for MOS targets", CreateInstance);
}

void ABISysV_mos::Terminate() {
  PluginManager::UnregisterPlugin(CreateInstance);
}

lldb::RegisterContextSP
ABISysV_mos::CreateRegisterContextForThread(lldb_private::Thread &thread,
                                            uint32_t concrete_frame_idx) const {
  LLDB_MOS_LOG_REG("[CreateRegisterContextForThread] ABI this={0}, map "
                   "addr={1}, map size={2}",
                   (void *)this, (void *)&imaginary_register_map,
                   imaginary_register_map.size());
  // Downcast to ThreadGDBRemote; safe in this context
  auto *gdb_thread =
      static_cast<lldb_private::process_gdb_remote::ThreadGDBRemote *>(&thread);
  assert(gdb_thread && "Expected ThreadGDBRemote for MOS ABI");
  // Use the same register info as the thread
  auto reg_info_sp = gdb_thread->GetRegisterInfoSP();
  bool read_all_registers_at_once = false; // Could be improved
  bool write_all_registers_at_once = false;
  return std::make_shared<MOSGDBRemoteRegisterContext>(
      *gdb_thread, concrete_frame_idx, reg_info_sp, read_all_registers_at_once,
      write_all_registers_at_once, GetImaginaryRegisterMap());
}

bool ABISysV_mos::ProvidesRegisterInfoOverride() const { return true; }

static lldb_private::DynamicRegisterInfo::Register
ConvertToDynamicRegisterInfoRegister(const RegisterInfo &reg) {
  lldb_private::DynamicRegisterInfo::Register dyn_reg;
  dyn_reg.name = ConstString(reg.name);
  dyn_reg.alt_name = ConstString(reg.alt_name ? reg.alt_name : "");
  dyn_reg.set_name = ConstString("general"); // or use actual set if available
  dyn_reg.byte_size = reg.byte_size;
  dyn_reg.byte_offset = reg.byte_offset;
  dyn_reg.encoding = reg.encoding;
  dyn_reg.format = reg.format;
  dyn_reg.regnum_dwarf = reg.kinds[eRegisterKindDWARF];
  dyn_reg.regnum_ehframe = reg.kinds[eRegisterKindEHFrame];
  dyn_reg.regnum_generic = reg.kinds[eRegisterKindGeneric];
  dyn_reg.regnum_remote = reg.kinds[eRegisterKindProcessPlugin];
  // value_regs, invalidate_regs, flags_type left default/empty
  return dyn_reg;
}

std::optional<lldb_private::DynamicRegisterInfo::Register>
ABISysV_mos::GetCanonicalRegisterInfo(llvm::StringRef name) const {
  for (size_t i = 0; i < std::size(g_register_infos); ++i) {
    const auto &reg = g_register_infos[i];
    if (name == reg.name || name == reg.alt_name) {
      auto dyn_reg = ConvertToDynamicRegisterInfoRegister(reg);
      dyn_reg.regnum_remote = i; // Set to index in static table
      return dyn_reg;
    }
  }
  // TODO: handle imaginary registers if needed
  return std::nullopt;
}

const std::unordered_map<std::string, lldb::addr_t> &
ABISysV_mos::GetImaginaryRegisterMap() const {
  return imaginary_register_map;
}

// Log the contents of the imaginary register map for debugging
void ABISysV_mos::LogImaginaryRegisterMap() const {
  for (const auto &pair : imaginary_register_map) {
    LLDB_MOS_LOG_REG("Imaginary register: {0} at address 0x{1:x}", pair.first,
                     pair.second);
  }
}
