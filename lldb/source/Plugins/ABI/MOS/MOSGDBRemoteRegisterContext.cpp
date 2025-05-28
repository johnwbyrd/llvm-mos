#include "MOSGDBRemoteRegisterContext.h"
#include "LLDBMOSLog.h"
#include "lldb/Core/Module.h"
#include "lldb/Symbol/Symbol.h"
#include "lldb/Symbol/Symtab.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Thread.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Utility/RegisterValue.h"
#include "lldb/lldb-enumerations.h"
#include <cstring>

MOSGDBRemoteRegisterContext::MOSGDBRemoteRegisterContext(
    lldb_private::process_gdb_remote::ThreadGDBRemote &thread, uint32_t concrete_frame_idx,
    lldb_private::process_gdb_remote::GDBRemoteDynamicRegisterInfoSP reg_info_sp, bool read_all_registers_at_once,
    bool write_all_registers_at_once,
    const std::unordered_map<std::string, lldb::addr_t> &imaginary_register_map)
    : GDBRemoteRegisterContext(thread, concrete_frame_idx, reg_info_sp,
                               read_all_registers_at_once,
                               write_all_registers_at_once),
      imaginary_register_map_(imaginary_register_map) {
  LLDB_MOS_LOG_REG("[MOSGDBRemoteRegisterContext ctor] this={0}, map addr={1}, map size={2}", (void*)this, (void*)&imaginary_register_map_, imaginary_register_map_.size());
  // Debug print all register names and sizes
  if (reg_info_sp) {
    LLDB_MOS_LOG_REG("RegisterInfo dump:");
    for (size_t i = 0; i < reg_info_sp->GetNumRegisters(); ++i) {
      const lldb_private::RegisterInfo *info = reg_info_sp->GetRegisterInfoAtIndex(i);
      if (info) {
        LLDB_MOS_LOG_REG(
            "  name='{0}', size={1}{2}", (info->name ? info->name : "(null)"),
            info->byte_size,
            (info->alt_name ? std::string(", alt_name='") + info->alt_name + "'"
                            : std::string()));
      }
    }
    // Dump the full register info table to stdout for debugging
    reg_info_sp->Dump();
  }
}

bool MOSGDBRemoteRegisterContext::IsImaginaryRegister(const lldb_private::RegisterInfo *reg_info) const {
  const char *name = reg_info->name;
  if (!name) return false;
  std::string sname(name);
  if (sname.size() > 2 && sname.substr(0,2) == "rc") {
    return imaginary_register_map_.count(sname) > 0;
  } else if (sname.size() > 2 && sname.substr(0,2) == "rs") {
    // rsN is valid if both rc[2*N] and rc[2*N+1] exist
    int n = std::atoi(sname.c_str() + 2);
    std::string rc_lo = "rc" + std::to_string(2*n);
    std::string rc_hi = "rc" + std::to_string(2*n+1);
    return imaginary_register_map_.count(rc_lo) > 0 && imaginary_register_map_.count(rc_hi) > 0;
  }
  return false;
}

bool MOSGDBRemoteRegisterContext::IsProcessReady() const {
  lldb::ProcessSP process_sp = m_thread.GetProcess();
  if (!process_sp) {
    LLDB_MOS_LOG_ABI("Not ready: no process");
    return false;
  }
  if (!process_sp->IsAlive()) {
    LLDB_MOS_LOG_ABI("Not ready: process is not alive");
    return false;
  }
  lldb::StateType state = process_sp->GetState();
  if (state != lldb::eStateStopped && state != lldb::eStateSuspended) {
    LLDB_MOS_LOG_ABI("Not ready: process state is {0}", state);
    return false;
  }
  if (process_sp->GetThreadList().GetSize() == 0) {
    LLDB_MOS_LOG_ABI("Not ready: no threads");
    return false;
  }
  return true;
}

bool MOSGDBRemoteRegisterContext::ReadRegister(
    const lldb_private::RegisterInfo *reg_info,
    lldb_private::RegisterValue &value) {
  LLDB_MOS_LOG_REG("[ReadRegister] RC this={0}, map addr={1}, map size={2}", (void*)this, (void*)&imaginary_register_map_, imaginary_register_map_.size());
  const char *name = reg_info->name;
  if (!name) return false;
  std::string sname(name);
  LLDB_MOS_LOG_REG("ReadRegister: looking up key '{}' in imaginary_register_map_ (size={})", sname, imaginary_register_map_.size());
  LLDB_MOS_LOG_REG("Imaginary register map contains:");
  for (const auto& pair : imaginary_register_map_) {
    LLDB_MOS_LOG_REG("  key='{}'", pair.first);
    if (pair.first == sname) {
      LLDB_MOS_LOG_REG("Direct comparison: '{}' == '{}' (MATCH)", pair.first, sname);
    } else {
      LLDB_MOS_LOG_REG("Direct comparison: '{}' != '{}'", pair.first, sname);
    }
  }
  if (sname.size() > 2 && sname.substr(0,2) == "rc") {
    auto it = imaginary_register_map_.find(sname);
    if (it == imaginary_register_map_.end()) {
      LLDB_MOS_LOG_REG("ReadRegister: lookup FAILED for key '{}'", sname);
      return false;
    }
    LLDB_MOS_LOG_REG("ReadRegister: lookup SUCCEEDED for key '{}', value=0x{:x}", sname, it->second);
    lldb_private::Status error;
    uint8_t byte = 0;
    size_t bytes_read = m_thread.GetProcess()->ReadMemory(it->second, &byte, 1, error);
    if (bytes_read == 1 && error.Success()) {
      value.SetUInt(byte, 1);
      return true;
    }
    LLDB_MOS_LOG_REG("Failed to read memory for {} at address 0x{:x}", sname, it->second);
    return false;
  } else if (sname.size() > 2 && sname.substr(0,2) == "rs") {
    int n = std::atoi(sname.c_str() + 2);
    std::string rc_lo = "rc" + std::to_string(2*n);
    std::string rc_hi = "rc" + std::to_string(2*n+1);
    auto it_lo = imaginary_register_map_.find(rc_lo);
    auto it_hi = imaginary_register_map_.find(rc_hi);
    if (it_lo == imaginary_register_map_.end() || it_hi == imaginary_register_map_.end()) {
      LLDB_MOS_LOG_REG("Failed to find {} or {} in imaginary_register_map_ for {}", rc_lo, rc_hi, sname);
      return false;
    }
    lldb_private::Status error;
    uint8_t lo = 0, hi = 0;
    LLDB_MOS_LOG_REG("Reading rs* register {} from rc_lo=0x{:x}, rc_hi=0x{:x}", sname, it_lo->second, it_hi->second);
    if (m_thread.GetProcess()->ReadMemory(it_lo->second, &lo, 1, error) != 1 || !error.Success()) {
      LLDB_MOS_LOG_REG("Failed to read memory for {} (lo) at address 0x{:x}", rc_lo, it_lo->second);
      return false;
    }
    if (m_thread.GetProcess()->ReadMemory(it_hi->second, &hi, 1, error) != 1 || !error.Success()) {
      LLDB_MOS_LOG_REG("Failed to read memory for {} (hi) at address 0x{:x}", rc_hi, it_hi->second);
      return false;
    }
    value.SetUInt(lo | (hi << 8), 2);
    return true;
  }
  // Fallback to base for hardware registers
  return GDBRemoteRegisterContext::ReadRegister(reg_info, value);
}

bool MOSGDBRemoteRegisterContext::WriteRegister(
    const lldb_private::RegisterInfo *reg_info,
    const lldb_private::RegisterValue &value) {
  if (!IsProcessReady()) return false;
  const char *reg_name = reg_info->name;
  std::string sname(reg_name ? reg_name : "");
  if (sname.size() > 2 && sname.substr(0,2) == "rc") {
    auto it = imaginary_register_map_.find(sname);
    if (it == imaginary_register_map_.end()) return false;
    uint8_t byte = value.GetAsUInt8();
    lldb_private::Status error;
    size_t bytes_written = m_thread.GetProcess()->WriteMemory(it->second, &byte, 1, error);
    return bytes_written == 1 && error.Success();
  } else if (sname.size() > 2 && sname.substr(0,2) == "rs") {
    int n = std::atoi(sname.c_str() + 2);
    std::string rc_lo = "rc" + std::to_string(2*n);
    std::string rc_hi = "rc" + std::to_string(2*n+1);
    auto it_lo = imaginary_register_map_.find(rc_lo);
    auto it_hi = imaginary_register_map_.find(rc_hi);
    if (it_lo == imaginary_register_map_.end() || it_hi == imaginary_register_map_.end()) return false;
    uint16_t val = value.GetAsUInt16();
    uint8_t lo = val & 0xFF, hi = (val >> 8) & 0xFF;
    lldb_private::Status error;
    if (m_thread.GetProcess()->WriteMemory(it_lo->second, &lo, 1, error) != 1 || !error.Success()) return false;
    if (m_thread.GetProcess()->WriteMemory(it_hi->second, &hi, 1, error) != 1 || !error.Success()) return false;
    return true;
  }
  // Fallback to base for hardware registers
  return GDBRemoteRegisterContext::WriteRegister(reg_info, value);
}