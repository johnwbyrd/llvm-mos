#include "MOSGDBRemoteRegisterContext.h"
#include "lldb/Core/Module.h"
#include "lldb/Symbol/Symbol.h"
#include "lldb/Symbol/Symtab.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Thread.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/RegisterValue.h"
#include "lldb/lldb-enumerations.h"
#include <cstring>

MOSGDBRemoteRegisterContext::MOSGDBRemoteRegisterContext(
    lldb_private::process_gdb_remote::ThreadGDBRemote &thread,
    uint32_t concrete_frame_idx,
    lldb_private::process_gdb_remote::GDBRemoteDynamicRegisterInfoSP
        reg_info_sp,
    bool read_all_registers_at_once, bool write_all_registers_at_once)
    : GDBRemoteRegisterContext(thread, concrete_frame_idx, reg_info_sp,
                               read_all_registers_at_once,
                               write_all_registers_at_once) {
  InitializeImaginaryRegisterMap();
  // Debug print all register names and sizes
  if (reg_info_sp) {
    if (lldb_private::Log *log =
            lldb_private::GetLog(lldb_private::LLDBLog::Process)) {
      LLDB_LOG(log, "[MOSGDBRemoteRegisterContext] RegisterInfo dump:");
    }
    for (size_t i = 0; i < reg_info_sp->GetNumRegisters(); ++i) {
      const lldb_private::RegisterInfo *info =
          reg_info_sp->GetRegisterInfoAtIndex(i);
      if (info) {
        if (lldb_private::Log *log =
                lldb_private::GetLog(lldb_private::LLDBLog::Process)) {
          LLDB_LOG(log, "  name='{0}', size={1}",
                   (info->name ? info->name : "(null)"), info->byte_size);
          if (info->alt_name)
            LLDB_LOG(log, ", alt_name='{0}'", info->alt_name);
        }
      }
    }
  }
}

void MOSGDBRemoteRegisterContext::InitializeImaginaryRegisterMap() {
  // Reference: llvm/lib/Target/MOS/MOSRegisterInfo.td
  // RC (8-bit) DWARF regnum: 0x10 + (N * 2)
  // RS (16-bit) DWARF regnum: 0x210 + N
  // Only __rc* symbols exist; RS registers are synthesized from RC pairs.

  m_imaginary_reg_addr.clear();
  m_fallback_to_hardware_sp =
      true; // Assume fallback unless we find imaginary regs

  lldb::ProcessSP process_sp = m_thread.GetProcess();
  if (!process_sp)
    return;
  lldb::TargetSP target_sp = process_sp->CalculateTarget();
  if (!target_sp)
    return;
  lldb_private::ModuleList &modules = target_sp->GetImages();

  // Map: rc index -> zero page address
  std::map<uint32_t, lldb::addr_t> rc_addr_map;
  constexpr uint32_t kMaxRC = 255;
  if (lldb_private::Log *log =
          lldb_private::GetLog(lldb_private::LLDBLog::Process)) {
    LLDB_LOG(
        log,
        "[MOSGDBRemoteRegisterContext] Scanning for imaginary registers...");
  }
  for (size_t i = 0; i < modules.GetSize(); ++i) {
    lldb::ModuleSP module_sp = modules.GetModuleAtIndex(i);
    if (!module_sp)
      continue;
    if (lldb_private::Log *log =
            lldb_private::GetLog(lldb_private::LLDBLog::Process)) {
      LLDB_LOG(log, "  Scanning module: {0}",
               module_sp->GetFileSpec().GetFilename().AsCString());
    }
    lldb_private::Symtab *symtab = module_sp->GetSymtab();
    if (!symtab)
      continue;
    for (uint32_t rc_num = 0; rc_num <= kMaxRC; ++rc_num) {
      std::string symbol_name = "__rc" + std::to_string(rc_num);
      lldb_private::ConstString cs_name(symbol_name.c_str());
      lldb_private::Symbol *symbol = symtab->FindFirstSymbolWithNameAndType(
          cs_name, lldb::eSymbolTypeAny, lldb_private::Symtab::eDebugAny,
          lldb_private::Symtab::eVisibilityAny);
      if (symbol) {
        lldb::addr_t raw_addr = symbol->GetRawValue();
        if (lldb_private::Log *log =
                lldb_private::GetLog(lldb_private::LLDBLog::Process)) {
          LLDB_LOG(log, "    Found {0} with raw value 0x{1:x}", symbol_name,
                   raw_addr);
        }
        if (raw_addr != LLDB_INVALID_ADDRESS)
          rc_addr_map[rc_num] = raw_addr;
      }
    }
  }
  if (lldb_private::Log *log =
          lldb_private::GetLog(lldb_private::LLDBLog::Process)) {
    LLDB_LOG(log, "Total rc* symbols found: {0}", rc_addr_map.size());
  }

  // Map RC registers (DWARF regnum 0x10 + N*2)
  for (const auto &pair : rc_addr_map) {
    uint32_t rc_num = pair.first;
    lldb::addr_t addr = pair.second;
    uint32_t dwarf_num = 0x10 + (rc_num * 2); // See MOSRegisterInfo.td
    m_imaginary_reg_addr[dwarf_num] = addr;
  }

  // Map RS registers (DWARF regnum 0x210 + N)
  constexpr uint32_t kMaxRS = kMaxRC / 2;
  for (uint32_t rs_num = 0; rs_num <= kMaxRS; ++rs_num) {
    uint32_t rc_lo = rs_num * 2;
    uint32_t rc_hi = rc_lo + 1;
    auto it_lo = rc_addr_map.find(rc_lo);
    auto it_hi = rc_addr_map.find(rc_hi);
    if (it_lo != rc_addr_map.end() && it_hi != rc_addr_map.end()) {
      uint32_t dwarf_num = 0x210 + rs_num;             // See MOSRegisterInfo.td
      m_imaginary_reg_addr[dwarf_num] = it_lo->second; // low byte address
    }
  }

  // If we found any imaginary registers, disable fallback
  if (!rc_addr_map.empty()) {
    m_fallback_to_hardware_sp = false;
  } else {
    if (lldb_private::Log *log =
            lldb_private::GetLog(lldb_private::LLDBLog::Process)) {
      LLDB_LOG(log, "[MOSGDBRemoteRegisterContext] Warning: No imaginary "
                    "registers found. Falling back to hardware S as 'sp'. "
                    "Source-level stack traces may not work.");
    }
  }
}

bool MOSGDBRemoteRegisterContext::IsImaginaryRegister(
    const lldb_private::RegisterInfo *reg_info) const {
  return m_imaginary_reg_addr.count(reg_info->kinds[lldb::eRegisterKindDWARF]) >
         0;
}

bool MOSGDBRemoteRegisterContext::IsProcessReady() const {
  lldb::ProcessSP process_sp = m_thread.GetProcess();
  if (!process_sp) {
    if (lldb_private::Log *log =
            lldb_private::GetLog(lldb_private::LLDBLog::Process)) {
      LLDB_LOG(log, "[MOSGDBRemoteRegisterContext] Not ready: no process");
    }
    return false;
  }
  if (!process_sp->IsAlive()) {
    if (lldb_private::Log *log =
            lldb_private::GetLog(lldb_private::LLDBLog::Process)) {
      LLDB_LOG(log,
               "[MOSGDBRemoteRegisterContext] Not ready: process is not alive");
    }
    return false;
  }
  lldb::StateType state = process_sp->GetState();
  if (state != lldb::eStateStopped && state != lldb::eStateSuspended) {
    if (lldb_private::Log *log =
            lldb_private::GetLog(lldb_private::LLDBLog::Process)) {
      LLDB_LOG(log,
               "[MOSGDBRemoteRegisterContext] Not ready: process state is {0}",
               state);
    }
    return false;
  }
  if (process_sp->GetThreadList().GetSize() == 0) {
    if (lldb_private::Log *log =
            lldb_private::GetLog(lldb_private::LLDBLog::Process)) {
      LLDB_LOG(log, "[MOSGDBRemoteRegisterContext] Not ready: no threads");
    }
    return false;
  }
  return true;
}

bool MOSGDBRemoteRegisterContext::ReadRegister(
    const lldb_private::RegisterInfo *reg_info,
    lldb_private::RegisterValue &value) {
  lldb_private::Log *log = lldb_private::GetLog(lldb_private::LLDBLog::Process);
  const char *reg_name = reg_info->name;
  uint32_t regnum = reg_info->kinds[lldb::eRegisterKindLLDB];
  if (log) {
    LLDB_LOG(log,
             "[MOSGDBRemoteRegisterContext] ReadRegister: name={0}, "
             "lldb_regnum={1}, generic_kind={2}, dwarf={3}",
             reg_name, regnum, reg_info->kinds[lldb::eRegisterKindGeneric],
             reg_info->kinds[lldb::eRegisterKindDWARF]);
  }
  if (!IsProcessReady()) {
    if (log) {
      LLDB_LOG(log,
               "[MOSGDBRemoteRegisterContext] ReadRegister: process not ready, "
               "returning false for {0}",
               reg_name);
    }
    return false;
  }
  // Handle 'sp' (soft stack pointer)
  if ((strcmp(reg_name, "sp") == 0 ||
       reg_info->kinds[lldb::eRegisterKindGeneric] == LLDB_REGNUM_GENERIC_SP)) {
    if (!m_fallback_to_hardware_sp) {
      // Map to RS0 (DWARF regnum for RS0 is 0x210)
      uint32_t rs0_dwarf = 0x210;
      auto it = m_imaginary_reg_addr.find(rs0_dwarf);
      if (it != m_imaginary_reg_addr.end()) {
        lldb_private::Status error;
        uint8_t buf[2] = {0};
        size_t bytes_read =
            m_thread.GetProcess()->ReadMemory(it->second, buf, 2, error);
        if (log) {
          LLDB_LOG(log,
                   "[MOSGDBRemoteRegisterContext] ReadRegister: reading sp "
                   "(RS0) from addr=0x{0:x}, bytes_read={1}, error={2}",
                   it->second, bytes_read,
                   error.Success() ? "none" : error.AsCString());
        }
        if (bytes_read == 2 && error.Success()) {
          value.SetFromMemoryData(*reg_info, buf, 2, lldb::eByteOrderLittle,
                                  error);
          if (log) {
            LLDB_LOG(log, "[MOSGDBRemoteRegisterContext] ReadRegister: handled "
                          "sp (RS0) successfully");
          }
          return true;
        } else {
          if (log) {
            LLDB_LOG(log, "[MOSGDBRemoteRegisterContext] ReadRegister: failed "
                          "to read sp (RS0) from memory");
          }
          return false;
        }
      }
    }
  }
  // Handle 'fp' (frame pointer, RS15)
  if ((strcmp(reg_name, "fp") == 0 ||
       reg_info->kinds[lldb::eRegisterKindGeneric] == LLDB_REGNUM_GENERIC_FP)) {
    uint32_t rs15_dwarf = 0x21f;
    auto it = m_imaginary_reg_addr.find(rs15_dwarf);
    if (it != m_imaginary_reg_addr.end()) {
      lldb_private::Status error;
      uint8_t buf[2] = {0};
      size_t bytes_read =
          m_thread.GetProcess()->ReadMemory(it->second, buf, 2, error);
      if (log) {
        LLDB_LOG(log,
                 "[MOSGDBRemoteRegisterContext] ReadRegister: reading fp "
                 "(RS15) from addr=0x{0:x}, bytes_read={1}, error={2}",
                 it->second, bytes_read,
                 error.Success() ? "none" : error.AsCString());
      }
      if (bytes_read == 2 && error.Success()) {
        value.SetFromMemoryData(*reg_info, buf, 2, lldb::eByteOrderLittle,
                                error);
        if (log) {
          LLDB_LOG(log, "[MOSGDBRemoteRegisterContext] ReadRegister: handled "
                        "fp (RS15) successfully");
        }
        return true;
      } else {
        if (log) {
          LLDB_LOG(log, "[MOSGDBRemoteRegisterContext] ReadRegister: failed to "
                        "read fp (RS15) from memory");
        }
        return false;
      }
    }
  }
  // Handle imaginary RC/RS registers
  if (IsImaginaryRegister(reg_info)) {
    auto it =
        m_imaginary_reg_addr.find(reg_info->kinds[lldb::eRegisterKindDWARF]);
    if (it != m_imaginary_reg_addr.end()) {
      lldb_private::Status error;
      size_t size = reg_info->byte_size;
      uint8_t buf[2] = {0};
      size_t bytes_read =
          m_thread.GetProcess()->ReadMemory(it->second, buf, size, error);
      if (log) {
        LLDB_LOG(
            log,
            "[MOSGDBRemoteRegisterContext] ReadRegister: reading imaginary reg "
            "{0} from addr=0x{1:x}, bytes_read={2}, error={3}",
            reg_name, it->second, bytes_read,
            error.Success() ? "none" : error.AsCString());
      }
      if (bytes_read == size && error.Success()) {
        value.SetFromMemoryData(*reg_info, buf, size, lldb::eByteOrderLittle,
                                error);
        if (log) {
          LLDB_LOG(log,
                   "[MOSGDBRemoteRegisterContext] ReadRegister: handled "
                   "imaginary reg {0} successfully",
                   reg_name);
        }
        return true;
      } else {
        if (log) {
          LLDB_LOG(log,
                   "[MOSGDBRemoteRegisterContext] ReadRegister: failed to read "
                   "imaginary reg {0} from memory",
                   reg_name);
        }
        return false;
      }
    }
  }
  // Fallback to base class
  if (log) {
    LLDB_LOG(log,
             "[MOSGDBRemoteRegisterContext] ReadRegister: falling back to base "
             "for {0}",
             reg_name);
  }
  bool result = GDBRemoteRegisterContext::ReadRegister(reg_info, value);
  if (log) {
    LLDB_LOG(log,
             "[MOSGDBRemoteRegisterContext] ReadRegister: base class returned "
             "{0} for {1}",
             result, reg_name);
  }
  return result;
}

bool MOSGDBRemoteRegisterContext::WriteRegister(
    const lldb_private::RegisterInfo *reg_info,
    const lldb_private::RegisterValue &value) {
  if (!IsProcessReady()) {
    if (lldb_private::Log *log =
            lldb_private::GetLog(lldb_private::LLDBLog::Process)) {
      LLDB_LOG(log, "[MOSGDBRemoteRegisterContext] Warning: WriteRegister "
                    "called when process is not ready");
    }
    return false;
  }
  lldb::ProcessSP process_sp = m_thread.GetProcess();
  if (!process_sp) {
    if (lldb_private::Log *log =
            lldb_private::GetLog(lldb_private::LLDBLog::Process)) {
      LLDB_LOG(log, "[MOSGDBRemoteRegisterContext] Error: Null process pointer "
                    "in WriteRegister");
    }
    return false;
  }
  if (!process_sp->IsAlive()) {
    if (lldb_private::Log *log =
            lldb_private::GetLog(lldb_private::LLDBLog::Process)) {
      LLDB_LOG(log, "[MOSGDBRemoteRegisterContext] Error: Process is not alive "
                    "in WriteRegister");
    }
    return false;
  }
  uint32_t dwarf_regnum = reg_info->kinds[lldb::eRegisterKindDWARF];
  const char *reg_name = reg_info->name ? reg_info->name : "(null)";
  size_t size = reg_info->byte_size;

  // Handle 'sp' (soft stack pointer)
  if ((strcmp(reg_name, "sp") == 0 ||
       reg_info->kinds[lldb::eRegisterKindGeneric] == LLDB_REGNUM_GENERIC_SP)) {
    if (!m_fallback_to_hardware_sp) {
      // Map to RS0 (DWARF regnum for RS0 is 0x210)
      uint32_t rs0_dwarf = 0x210;
      auto it = m_imaginary_reg_addr.find(rs0_dwarf);
      if (it != m_imaginary_reg_addr.end()) {
        lldb_private::Status error;
        uint8_t buf[2] = {0};
        value.GetAsMemoryData(*reg_info, buf, size, process_sp->GetByteOrder(),
                              error);
        if (!error.Success())
          return false;
        size_t bytes_written =
            process_sp->WriteMemory(it->second, buf, size, error);
        return bytes_written == size && error.Success();
      }
      return false;
    } else {
      // Fallback: use hardware S (only low byte is written)
      const lldb_private::RegisterInfo *s_info = nullptr;
      for (size_t i = 0; i < m_reg_info_sp->GetNumRegisters(); ++i) {
        const auto *info = m_reg_info_sp->GetRegisterInfoAtIndex(i);
        if (info && strcmp(info->name, "s") == 0) {
          s_info = info;
          break;
        }
      }
      if (!s_info)
        return false;
      uint8_t buf[2] = {0};
      lldb_private::Status error;
      value.GetAsMemoryData(*reg_info, buf, 2, process_sp->GetByteOrder(),
                            error);
      if (!error.Success())
        return false;
      lldb_private::RegisterValue s_val;
      s_val.SetUInt8(buf[0]);
      return GDBRemoteRegisterContext::WriteRegister(s_info, s_val);
    }
  }
  // Handle 'fp' (soft frame pointer)
  if ((strcmp(reg_name, "fp") == 0 ||
       reg_info->kinds[lldb::eRegisterKindGeneric] == LLDB_REGNUM_GENERIC_FP)) {
    if (!m_fallback_to_hardware_sp) {
      // Map to RS15 (DWARF regnum for RS15 is 0x210 + 15)
      uint32_t rs15_dwarf = 0x210 + 15;
      auto it = m_imaginary_reg_addr.find(rs15_dwarf);
      if (it != m_imaginary_reg_addr.end()) {
        lldb_private::Status error;
        uint8_t buf[2] = {0};
        value.GetAsMemoryData(*reg_info, buf, size, process_sp->GetByteOrder(),
                              error);
        if (!error.Success())
          return false;
        size_t bytes_written =
            process_sp->WriteMemory(it->second, buf, size, error);
        return bytes_written == size && error.Success();
      }
      return false;
    } else {
      if (lldb_private::Log *log =
              lldb_private::GetLog(lldb_private::LLDBLog::Process)) {
        LLDB_LOG(log, "[MOSGDBRemoteRegisterContext] Warning: No imaginary "
                      "registers; 'fp' is not available.");
      }
      return false;
    }
  }
  // Handle 's' (hardware stack pointer)
  if (strcmp(reg_name, "s") == 0) {
    return GDBRemoteRegisterContext::WriteRegister(reg_info, value);
  }
  // Handle imaginary registers
  if (IsImaginaryRegister(reg_info)) {
    lldb::addr_t addr = m_imaginary_reg_addr.at(dwarf_regnum);
    lldb_private::Status error;
    uint8_t buf[2] = {0};
    value.GetAsMemoryData(*reg_info, buf, size, process_sp->GetByteOrder(),
                          error);
    if (!error.Success())
      return false;
    size_t bytes_written = process_sp->WriteMemory(addr, buf, size, error);
    return bytes_written == size && error.Success();
  }
  // Handle real registers (A, X, Y, P, etc.)
  if (strcmp(reg_name, "a") == 0 || strcmp(reg_name, "x") == 0 ||
      strcmp(reg_name, "y") == 0 || strcmp(reg_name, "p") == 0) {
    return GDBRemoteRegisterContext::WriteRegister(reg_info, value);
  }
  // Unknown register
  return false;
}